ELD  (debugging guide)
=========================

This document describes the high-level flow of how ELD executes a link, with
call-site pointers and practical tips for debugging failures.

.. contents::
   :local:


Big picture
-----------

At a high level, a link invocation looks like this:

1. **eld main** expands response files and selects a driver flavor/target.
2. **Driver** parses options and builds an ordered list of input actions.
3. **Linker prepare** initializes target/emulation, inputs, and plugins, then
   reads and normalizes input files (and may run LTO).
4. **Linker link** resolves symbols/relocations, lays out output sections, then
   emits the final ELF and optional map files.
5. **Optional diagnostics**: reproduce tarball/mapping file, plugin activity
   log, timing stats, summary, etc.


Entry point and driver selection
--------------------------------

The executable entry point is ``tools/eld/eld.cpp``:

* Expands ``@response`` files via ``llvm::cl::ExpandResponseFiles(...)``.
* Creates a ``Driver`` and calls
  ``Driver::setDriverFlavorAndInferredArchFromLinkCommand(...)``.
* Creates a GNU-ld-compatible driver (``GnuLdDriver``) and calls
  ``GnuLdDriver::link(...)``.

Driver flavor selection is implemented in ``lib/LinkerWrapper/Driver.cpp``:

* First tries to infer a target from the program name (e.g. ``arm-link``,
  ``aarch64-link``, ``hexagon-link``).
* Otherwise inspects early arguments like ``-m <emulation>`` or ``-march`` to
  select a target-specific driver.

Environment hooks that affect arguments:

* ``ELDFLAGS``: appended to the link command by the driver (useful for always-on
  debug flags).


Argument parsing and preprocessing
----------------------------------

The top-level flow of option parsing and dispatch is in
``lib/LinkerWrapper/GnuLdDriver.cpp`` (``GnuLdDriver::link(...)``):

1. ``parseOptions(...)``
2. ``processLLVMOptions(...)`` (parses ``-mllvm ...`` arguments)
3. ``processTargetOptions(...)`` (handles ``-mtriple``, ``-march``, ``-mabi``,
   ``-m <emulation>``, etc.)
4. ``processOptions(...)`` (general linker options)
5. ``checkOptions(...)`` and ``overrideOptions(...)``
6. ``createInputActions(...)`` to build the ordered action list
7. ``doLink(...)`` to run the actual link pipeline

If you suspect argument/option issues, start with:

* ``--verbose`` (or ``--verbose=<level>``)
* ``--trace=command-line``
* ``--trace=files`` or ``-t`` (prints processed files)
* ``--error-style=GNU`` or ``--error-style=LLVM`` (if output formatting matters)


Input actions (what gets fed to the linker)
-------------------------------------------

After parsing options, the driver builds a sequence of actions that are later
"activated" to create inputs:

* ``-T <script>`` / ``--default-script <script>`` -> ``ScriptAction``
* ``-R <file>`` -> ``JustSymbolsAction``
* ``--defsym <sym>=<expr>`` -> ``DefSymAction``
* ``-l <namespec>`` -> ``NamespecAction`` (library search)
* Plain inputs -> ``InputFileAction``
* State toggles like ``--whole-archive``, ``--as-needed``,
  ``--start-group/--end-group``, ``--start-lib/--end-lib`` -> corresponding
  actions that affect how subsequent inputs are treated

This happens in ``GnuLdDriver::createInputActions(...)`` in
``lib/LinkerWrapper/GnuLdDriver.cpp``.

Debug tip: if you see failures about mismatched groups/libs, the error is
detected here (before any ELF parsing starts).


Link pipeline overview (doLink -> Linker)
-----------------------------------------

Once actions are created, ``GnuLdDriver::doLink(...)`` does the setup:

* Looks up the LLVM target and the ELD target based on the chosen triple.
* Creates a ``Module`` (``lib/Core/Module.cpp``) and optional ``LayoutInfo`` for
  map printing.
* Selects map printers based on ``--MapStyle=...`` (or defaults) and prepares
  layout printers.
* Constructs an ``eld::Linker`` and runs:
  * ``linker.prepare(actions, target)``
  * ``linker.link()`` (unless running "LTO-only" modes)
  * ``linker.printLayout()`` (map file emission)
* Runs plugin teardown hooks, unloads plugins, emits stats, and finalizes the
  diagnostic engine summary.


Prepare phase (Linker::prepare)
-------------------------------

``Linker::prepare(...)`` (``lib/Core/Linker.cpp``) is responsible for:

1. **Target/emulation + backend**

   * Initializes emulator and backend for the selected target.
2. **Initialize inputs**

   * Builds the input tree, creates internal linker-generated inputs, activates
     the action list, and reads linker scripts.
3. **Universal plugins**

   * Reads plugin configuration, loads universal plugins from the script, stores
     them, and runs plugin init hooks.
4. **Read/normalize inputs**

   * Reads all input files, sections, symbols, and (optionally) runs LTO-related
     preprocessing.

Common debug levers for this phase:

* ``--trace=linker-script`` or ``--trace-linker-script`` (script parsing)
* ``--trace=threads`` (parallel input/section reading behavior)
* ``--trace=LTO`` or ``--trace-lto`` (LTO stage boundaries)
* ``--plugin-config=<config-file>`` and ``--no-default-plugins`` (plugin triage)


Normalize phase (Linker::normalize)
-----------------------------------

``Linker::normalize()`` (``lib/Core/Linker.cpp``) performs:

* Optional command-line header/summary printing when ``--trace=command-line`` is
  enabled (via ``LinkerConfig::printOptions(...)`` in ``lib/Config/LinkerConfig.cpp``).
* Reading all input files via ``ObjLinker->normalize()``:
  * Parses ELF objects/archives/shared libraries/bitcode inputs.
  * Populates symbol tables and initial symbol resolution.
* Loads non-universal plugins.
* Computes code position (static/dynamic/PIE) and validates incompatible
  options (e.g. patch options with non-static output).
* Parses external scripts:

  * Version scripts
  * Dynamic list (when building dynamic artifacts)
* Adds linker-script-defined symbols.
* LTO steps (when needed):

  * Creates an LTO object from bitcode inputs.
  * Re-runs normalization post-LTO after replacing bitcode with generated
    objects.

Debug tip: LTO-related failures often reproduce reliably with ``--reproduce``
because the tarball will include bitcode inputs and any generated LTO objects
recorded by the linker.


Resolve + layout + emit (Linker::link)
--------------------------------------

``Linker::link()`` in ``lib/Core/Linker.cpp`` is the main "work" phase:

1. **Standard sections**

   * Initializes default sections (and per-file synthetic dynamic sections when
     producing dynamic outputs).
2. **Resolve (symbol/reloc processing)**

   * Reads relocations.
   * Allocates common symbols.
   * Assigns output sections using default + linker script rules.
   * Runs plugin hooks around rule matching/layout.
   * Processes target-specific input handling.
   * Optionally performs garbage collection / stripping.
   * Scans relocations, finalizes scan results, and builds output/dynamic symbol
     tables.
   * Merges sections and creates section symbols.
3. **Layout**

   * Initializes stubs/trampolines, prelayout, merge-strings optimization.
   * Establishes final layout and postlayout output section table.
   * Finalizes symbol values, runs output-section iterator plugins, applies
     relocations, and finalizes output state.
4. **Emit**

   * Computes output file size and creates an ``llvm::FileOutputBuffer``.
   * Writes section contents, performs post-processing, emits Build ID, commits
     the output, and optionally verifies the output size on disk.

If a failure happens late (layout/emit), map files are usually the fastest way
to pinpoint the problematic section/segment/relocation.


Internal inputs and "internal sections"
---------------------------------------

ELD creates a number of linker-generated inputs up front so later stages can
treat them uniformly as normal inputs/sections/symbols.

Creation happens in ``Module::createInternalInputs()`` (``lib/Core/Module.cpp``).
Each internal input corresponds to a named ``Input``/``InputFile`` (for example
``Attributes``, ``CommonSymbols``, ``DynamicSections``, ``Trampoline``, and
others) and is used to host sections/fragments that are not sourced from a user
object file.

Two other "internal" concepts are easy to confuse:

* **Linker-internal input sections**: sections owned by an internal input file
  (``Input::Internal``). These typically have ``LDFileFormat::Internal`` kind
  and may carry relocations to be applied later.
* **Output-format sections**: sections that come from the backend/output format
  (not from a user input file), for example dynamic tables/headers. These are
  treated as output sections and can be matched/discarded via linker-script
  rules; see ``ObjectLinker::markDiscardFileFormatSections()`` in
  ``lib/Object/ObjectLinker.cpp``.

Debug tips:

* If you suspect an unexpected section exists (or is missing), prefer a text
  map: ``-M --MapStyle=Text --Map=<file>``.
* If a section is unexpectedly discarded, use ``--trace-section <name>`` and
  check whether it matched a ``/DISCARD/`` rule.


Section merging (input sections -> output sections)
---------------------------------------------------

The "merge sections" name in ELD means: take the *input* section graph (from
object files + internal inputs), and populate the *output* section layout
according to default rules + linker-script rules + plugins.

There are three distinct sub-steps to keep straight:

1. **Rule matching / output section assignment**

   * ``ObjectLinker::assignOutputSections(...)`` (``lib/Object/ObjectLinker.cpp``)
     uses an ``ObjectBuilder`` to match input sections against linker-script
     rules (including wildcards, sorting policies, ``EXCLUDE_FILE``, etc).
2. **Input section merging**

   * ``ObjectLinker::mergeSections()`` calls ``mergeInputSections(...)`` which
     iterates all input sections and merges them into output sections, with
     special handling for some section kinds (``.eh_frame``, ``.sframe``, target
     overrides, linkonce/reloc sections, etc).
3. **Finalize output sections**

   * ``createOutputSection(...)`` builds each output section's fragment list,
     computes flags/alignment, assigns fragment offsets, and inserts the output
     sections into the module's output section table.

Special-case section handling during merging
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``ObjectLinker::mergeInputSections(...)`` (``lib/Object/ObjectLinker.cpp``)
handles some input section kinds specially:

* ``LDFileFormat::Relocation`` and ``LDFileFormat::LinkOnce``: if the "link"
  section is discarded/ignored, the relocation/linkonce section is ignored too.
* ``LDFileFormat::Target``: backends may override merging via
  ``GNULDBackend::DoesOverrideMerge(...)`` and ``GNULDBackend::mergeSection(...)``.
* ``LDFileFormat::EhFrame``:

  * Splits and re-chunks ``.eh_frame`` into CIE/FDE fragments.
  * If enabled, registers content for ``.eh_frame_hdr`` and creates filler/hdr
    fragments in the backend.
* ``LDFileFormat::SFrame``:

  * Parses the section and may create an ``SFrame`` header fragment when
    configured.

Everything else typically flows through ``ObjectBuilder::mergeSection(...)`` and
ends up contributing fragments to an output section.

Output section construction and offsets
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Once merging decides *which* fragments belong to a particular output section,
``ObjectLinker::createOutputSection(...)`` and ``ObjectLinker::assignOffset(...)``
lay them out:

* Output section ``ALIGN`` / input section ``SUBALIGN`` (from linker script) is
  enforced when present.
* "Dirty" rules (modified by plugins) trigger a recomputation of input section
  flags/type/align based on the fragments that ended up in the rule.
* Fragment offsets are assigned linearly; per-fragment padding/alignment is
  applied by ``Fragment::paddingSize()`` (``lib/Fragment/Fragment.cpp``).

Debug tips:

* If you see "offset not assigned" diagnostics, the fragment/section likely
  never got placed into an output section (or got discarded). The diagnostic
  plumbing is in ``Fragment::getOffset(...)`` (``lib/Fragment/Fragment.cpp``).
* If rule sorting changes layout unexpectedly, check whether the linker script
  wildcard includes a sort policy, or whether ``--sort-section=...`` is enabled.


String merging (MergeString fragments)
--------------------------------------

String merging is a dedicated optimization pass that runs during layout, before
final output layout is established:

* ``ObjectLinker::doMergeStrings()`` calls:

  * ``mergeIdenticalStrings()``: merges ``MergeStringFragment`` content (can be
    threaded across output sections; global non-alloc merge is done single-threaded).
  * ``fixMergeStringRelocations()``: updates relocations that refer into merged
    strings via ``Relocator::doMergeStrings(...)``.

The output offset calculation for merged strings is special-cased in
``FragmentRef::getOutputOffset(...)`` (``lib/Fragment/FragmentRef.cpp``), because
multiple input strings may map to a shared output string (including suffix
merging).

Debug tips:

* Use ``--trace=merge-strings`` / ``--trace-merge-strings=<option>`` to see why
  strings were merged and how offsets were computed.


Relocations: read -> scan -> apply -> (optional) emit reloc sections
--------------------------------------------------------------------

There are multiple relocation passes, and confusing them is a common source of
"where did this relocation come from?" debugging pain.

Read relocations
^^^^^^^^^^^^^^^^

``ObjectLinker::readRelocations()`` reads relocations from input objects
(``lib/Object/ObjectLinker.cpp``):

* Skips non-object inputs, and skips inputs marked "just symbols".
* For patch-base inputs, runs patch-base parsing via the executable-object parser.

Scan relocations (reservation / planning)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``ObjectLinker::scanRelocations(...)`` (``lib/Object/ObjectLinker.cpp``) is the
"planning" pass. It typically:

* Invokes ``Relocator::scanRelocation(...)`` per relocation, which is where the
  backend decides whether it needs to reserve GOT/PLT entries, create or reserve
  dynamic relocations, create stubs/trampolines, etc. (target-specific logic is
  in ``lib/Target/*/*Relocator.cpp``).
* Collects copy-relocation candidates per input, then creates copy relocations
  once per symbol (see ``createCopyRelocation(...)`` / ``addCopyReloc(...)`` in
  ``lib/Object/ObjectLinker.cpp``).
* Merges per-file dynamic relocation vectors into a single "reloc input"
  (``getDynamicSectionHeadersInputFile()``) so later code can treat them
  consistently.
* Runs ``ObjectLinker::finalizeScanRelocations()`` which calls
  ``GNULDBackend::finalizeScanRelocations()`` for backend-specific finalization.

In relocatable/partial links, ELD uses ``partialScanRelocation(...)`` instead.

Create output relocation sections (``--emit-relocs``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If ``--emit-relocs`` is enabled, ELD creates output relocation sections during
prelayout:

* ``ObjectLinker::prelayout()`` calls ``createRelocationSections()``.
* ``createRelocationSections()`` counts relocations per output section and
  creates the corresponding output relocation sections (``.rel.<sec>`` /
  ``.rela.<sec>`` style, based on target) sized to hold all entries.

Apply relocations (writes relocation results)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Relocation application happens in ``ObjectLinker::relocation(...)``:

* Applies internal/linker-created relocations.
* Applies input relocations, skipping relocations that are known to be relaxed
  or that target discarded/ignored sections/symbols.
* Applies branch-island (relaxation) relocations after input relocations are
  applied.
* If ``--emit-relocs`` is enabled, emits external-form relocation records into
  the output relocation sections (via ``EmitOneReloc``).

Finally, ``syncRelocations(...)`` writes relocation results into the output
buffer, including extra ordering/barriers to avoid races when multi-threaded.

Debug tips:

* ``--trace=reloc=<pattern>`` pinpoints a single relocation kind.
* ``--trace=symbol=<name>`` helps tie relocations back to symbol resolution.
* If you see overflows/unencodable immediates, diagnostics originate from
  ``Relocation::issueSignedOverflow(...)`` / ``issueUnencodableImmediate(...)``
  in ``lib/Readers/Relocation.cpp``.


Dynamic relocations (what creates ``.rel[a].dyn`` / ``.rel[a].plt``)
-------------------------------------------------------------------

Dynamic relocation entries are typically created/reserved during the relocation
scan phase inside the target relocator and backend:

* Target relocators decide whether a given relocation needs:
  * a static relocation only,
  * a dynamic relocation (REL/RELA),
  * a PLT/GOT entry (and an associated relocation),
  * a copy relocation (for executable data symbol imports).
* Backends provide the actual sections for dynamic relocations (for example
  ``.rela.dyn`` / ``.rela.plt``) and may sort/finalize them. A common set of
  helper logic lives in ``lib/Target/GNULDBackend.cpp``.

You can generally think of the relocation scan as "reserving and populating
dynamic relocation vectors", and layout/emission as "placing and writing those
sections".


Garbage collection (``--gc-sections``)
--------------------------------------

Garbage collection in ELD is graph reachability over sections, built from
relocations and a chosen root set ("entry sections").

Where it runs
^^^^^^^^^^^^^

The default GC pass is triggered during the resolve phase:

* ``ObjectLinker::dataStrippingOpt()`` checks
  ``IRBuilder::shouldRunGarbageCollection()`` and calls
  ``ObjectLinker::runGarbageCollection(\"GC\")``.
* The implementation is ``GarbageCollection`` in
  ``lib/GarbageCollection/GarbageCollection.cpp``.

How the graph is built
^^^^^^^^^^^^^^^^^^^^^^

``GarbageCollection::setUpReachedSectionsAndSymbols()``:

* Traverses input relocations and records, per "apply section", the set of
  reachable target sections and reachable symbols.
* Handles special cases:

  * Script-defined symbols: walks the assignment expression's symbol references.
  * Magic ``__start_*/__stop_*`` symbols: forces sections with matching names
    into the reachable set.
  * Bitcode: defers some reachability until it can map referenced symbols back
    to bitcode "input sections".
* Allows the backend to add extra reachability via
  ``GNULDBackend::setUpReachedSectionsForGC(...)``.

How entry sections are chosen
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``GarbageCollection::getEntrySections()`` considers multiple root sources:

* The configured entry symbol (if it resolves to a fragment).
* Sections matched by ``KEEP(...)`` in the linker script.
* When producing dynamic outputs, exported/visible symbols (subject to version
  script scoping) contribute entry sections.
* Sections marked with ``SHF_GNU_RETAIN`` are treated as entry-like.

Mark-and-sweep
^^^^^^^^^^^^^^

``GarbageCollection::findReferencedSectionsAndSymbols(...)`` performs a BFS from
entry sections, following the reachability map built earlier, producing a live
set. ``stripSections(...)`` then marks sections not in the live set as ignored
(and can optionally print what got collected).

Debug tips:

* ``--print-gc-sections`` shows what got collected.
* ``--trace=garbage-collection`` and ``--trace=live-edges`` are useful when a
  section is unexpectedly dead/alive.
* If GC keeps/drops a zero-sized section unexpectedly, check whether it is the
  target of a relocation or contains a symbol (see the FAQ discussion of
  zero-sized sections).


Fragment model (Fragment / FragmentRef)
---------------------------------------

ELD uses a fragment model internally where **fragments are the minimum linking
unit**, not sections.

Fragments
^^^^^^^^^

``Fragment`` (``include/eld/Fragment/Fragment.h``) represents a typed chunk of
content that will appear in the output. Examples include:

* raw data regions (region fragments),
* stubs / trampolines / branch island content,
* GOT / PLT entries,
* mergeable strings,
* ``.eh_frame``-related pieces (CIE/FDE fragments),
* build-id fragments, timing fragments, and others.

Each fragment belongs to an owning (input) section and has:

* an alignment requirement,
* an assigned (unaligned) offset, and derived padding size,
* an ``emit(...)`` implementation that writes bytes during output generation.

Offsets, padding, and "why is this input marked used?"
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

During output section construction, ELD assigns fragment offsets linearly. The
final effective offset includes per-fragment padding computed by
``Fragment::paddingSize()`` (``lib/Fragment/Fragment.cpp``). When a fragment
offset is assigned, ``Fragment::setOffset(...)`` also marks the owning input as
"used" when it contributes allocatable content (this feeds into GC and
diagnostics).

FragmentRef (symbol/relocation addressing)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``FragmentRef`` (``include/eld/Fragment/FragmentRef.h``) is a pointer to:

* a fragment, plus
* an additional byte offset within that fragment.

This is the core indirection used by:

* output symbols (symbols carry a ``FragmentRef`` to their definition),
* relocations (relocation "place" and/or "target" is a ``FragmentRef``).

Output offset computation is not always "fragment offset + ref offset":

* ``FragmentRef::getOutputOffset(...)`` special-cases ``.eh_frame`` to map
  offsets through the split/piece layout.
* It also special-cases merged strings so references land on the deduplicated
  output string (including suffix merging).

Debug tips:

* If a relocation points somewhere surprising, inspect:
  * the relocation's ``targetRef`` (place), and
  * the symbol's ``fragRef`` (definition),
  and remember both can have special-cased output offset behavior.


Map files (layout printers)
---------------------------

Map emission is handled after the link attempt in ``Linker::printLayout()`` and
also from the crash signal handler.

Key options:

* ``-M`` / ``--print-map``: enable map generation
* ``--Map=<filename>``: choose map output file
* ``--MapStyle=<YAML|Text|Binary>``: choose format(s)
* ``--MapDetail=<option>``: more detail in maps
* ``--color-map``: colorize map output
* ``--trampoline-map <filename>``: trampoline information (YAML)


Reproducing failures (tarball + mapping file)
---------------------------------------------

ELD can capture a self-contained reproducer for link issues:

* ``--reproduce <tarfilename>|default``: always produce a tarball;
  use ``--reproduce=default`` to name the tar after the output file
  (``<output>.tar``, where ``<output>`` is the ``-o`` filename, or
  ``a.out.tar`` if ``-o`` is not given)
* ``--reproduce-compressed <tarfilename>``: compressed tarball
* ``--reproduce-on-fail <tarfilename>|default``: only on failure;
  use ``--reproduce-on-fail=default`` to name the tar after the output file
* ``ELD_REPRODUCE_CREATE_TAR``: environment variable that forces reproducer
  creation (uses a temporary tar file if no filename is provided)

Additional reproduce helpers:

* ``--mapping-file <INI-file>``: reproduce link using a mapping file
* ``--dump-mapping-file <outputfilename>``: dump mapping info
* ``--dump-response-file <outputfilename>``: dump rewritten response file

The reproduce tarball logic is wired through:

* ``GnuLdDriver::handleReproduce(...)`` and ``writeReproduceTar(...)`` in
  ``lib/LinkerWrapper/GnuLdDriver.cpp``
* ``Module::createOutputTarWriter()`` creation decision via
  ``LinkerConfig::shouldCreateReproduceTar()`` (``lib/Config/LinkerConfig.cpp``)


Crash/signal behavior (what gets written on a crash)
----------------------------------------------------

ELD installs a default signal handler in ``GnuLdDriver::doLink(...)``:

* Flushes a text map file (if configured).
* Detects likely plugin crashes and reports them.
* Writes a temporary ``.sh`` script that appends ``--reproduce=default``
  to the original command line; when the user reruns that script the tarball
  is named ``<output>.tar`` based on the ``-o`` output filename.

This is implemented in ``GnuLdDriver::defaultSignalHandler(...)`` in
``lib/LinkerWrapper/GnuLdDriver.cpp``.


Where failures typically come from (symptoms -> pipeline stage)
--------------------------------------------------------------

This section is meant as a quick index: if you see a symptom, these are the
stages/files to inspect first. Many of these topics are also discussed in more
detail in ``docs/userguide/documentation/linker_faq.rst``.

Driver/target selection failures
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Symptoms:

* "unsupported emulation" / "cannot find target"
* wrong backend chosen when using ``-m`` / ``-march``

Start here:

* ``Driver::getDriverFlavorFromLinkCommand(...)`` in ``lib/LinkerWrapper/Driver.cpp``
* ``GnuLdDriver::processTargetOptions(...)`` and target lookups in
  ``GnuLdDriver::doLink(...)`` (``lib/LinkerWrapper/GnuLdDriver.cpp``)

Input specification / archive/group issues
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Symptoms:

* "mismatched group" / "mismatched lib"
* unexpected missing objects from an archive

Start here:

* ``GnuLdDriver::createInputActions(...)`` for ``--start-group``/``--end-group``
  and ``--start-lib``/``--end-lib`` balancing and ordering
* Use ``-t`` / ``--trace=files`` to confirm what ELD actually processed

Linker script rule-matching errors
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Symptoms:

* "no linker script rule for \".bss\"" / "\.data.bar" style errors
* sections landing in unexpected output sections

Start here:

* ``ObjectLinker::assignOutputSections(...)`` and friends in
  ``lib/Object/ObjectLinker.cpp``
* Emit a map file and confirm whether the input section matched any rule; the
  FAQ has a guide for diagnosing these errors and for finding used/unused rules.
* If this is a *script parsing/syntax* problem (rather than rule matching),
  enable ``--trace=linker-script`` / ``--trace-linker-script`` and use
  ``--reproduce[-on-fail]`` to capture the exact script(s) and rewritten command
  line that ELD used.

Practical tips:

* Reduce the script: comment out most rules and add back until the behavior
  flips. For rule-matching bugs, keep only the relevant ``SECTIONS`` rules and a
  minimal ``MEMORY`` map.
* Prefer map files while iterating: they show which input section went to which
  output section and why that changed across experiments.

Linker script parsing and evaluation errors
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Symptoms:

* parse errors (unexpected token, unexpected ``)``, etc)
* script expression issues (undefined symbol in an expression, unexpected value)
* placement issues that are script-driven (for example: region overflow, PHDR
  mismatch, or a section being forced into an incompatible segment)

Start here:

* ``--trace=linker-script`` / ``--trace-linker-script`` to see script parsing,
  includes, and key evaluation decisions.
* A text map file to confirm what the script actually did: memory regions,
  output section addresses, and segment layout are usually visible there.
* ``--reproduce[-on-fail]`` so the exact script(s) used by the link are captured
  alongside the rewritten response file (this is critical when scripts are
  generated by the build system).

Undefined references and symbol resolution surprises
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Symptoms:

* "undefined reference" failures
* symbol unexpectedly resolved from a different archive/object

Start here:

* Resolve phase in ``Linker::resolve()`` (``lib/Core/Linker.cpp``) and the
  diagnostic engine output
* Use ``--trace=symbol=<name>`` (or ``--trace=all-symbols``) to see the
  resolution path

When the failure is a *runtime* crash (not a link error), symbol resolution can
still be the root cause:

* Wrong interposition/visibility (a symbol resolves but to an unexpected
  definition at runtime).
* Lazy binding via PLT (a crash happens on the first call into a function that
  is resolved late by the dynamic loader).

Quick inspections:

.. code-block:: sh

   # Symbol tables (static and dynamic), with type/binding/visibility:
   llvm-readelf -s --demangle --extra-sym-info ./app | less
   llvm-readelf -s --demangle --extra-sym-info ./libfoo.so | less

   # Dynamic symbols only (what the runtime loader sees):
   llvm-readelf --dyn-syms --demangle --extra-sym-info ./libfoo.so | less

   # Undefined dynamic symbols (what must be provided by dependencies):
   llvm-readelf --dyn-syms --demangle --extra-sym-info ./libfoo.so | awk '$7==\"UND\" {print}'

   # Imported/Exported symbols (alternate views):
   nm -D --defined-only ./libfoo.so | less
   nm -D --undefined-only ./libfoo.so | less

Garbage collection removed something needed
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Symptoms:

* function/data present in inputs but missing from output
* a section disappears only with ``--gc-sections``

Start here:

* ``GarbageCollection`` implementation in ``lib/GarbageCollection/GarbageCollection.cpp``
* ``--print-gc-sections`` plus ``--trace=garbage-collection`` / ``--trace=live-edges``
* Ensure linker-script ``KEEP(...)`` is used for sections that must never be GC'd

Relocation overflows / unencodable immediates / target-specific relocation bugs
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Symptoms:

* overflow/unencodable relocation diagnostics
* crashes during relocation scan/apply
* output runs but has wrong addresses at runtime

Start here:

* Scan phase: ``ObjectLinker::scanRelocations(...)`` and target relocators
  (``lib/Target/*/*Relocator.cpp``)
* Apply phase: ``ObjectLinker::relocation(...)`` and sync/writeback
  (``lib/Object/ObjectLinker.cpp``)
* Diagnostics: ``lib/Readers/Relocation.cpp`` (location printing, overflow, etc)

Trampolines / stubs / relaxation issues
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Symptoms:

* failures mentioning trampolines, far calls, or branch islands
* layout changes causing new trampolines or changing trampoline reuse

Start here:

* ``ObjectLinker::initStubs()`` and target stub factories/backends
* Map/trampoline map options (``--trampoline-map``) plus FAQ sections on
  trampoline naming and reuse controls

LTO failures
^^^^^^^^^^^^

Symptoms:

* failures only with ``-flto`` / ThinLTO / Full LTO
* "LTO merge error" / codegen diagnostics

Start here:

* ``ObjectLinker::createLTOObject()`` and LTO diagnostics handler in
  ``lib/Object/ObjectLinker.cpp``
* ``--trace=LTO`` / ``--trace-lto`` and ``--reproduce[-on-fail]`` to capture
  inputs and generated objects
* ``--save-temps`` (or ``--save-temps=<dir>``) to preserve intermediate LTO
  artifacts for inspection (files use the prefix ``<output>.llvm-lto.*``)
* If you need stable, non-temporary LTO-generated objects: ``--lto-obj-path=<prefix>``
  (also keeps the objects from being deleted after LTO)

Plugin-caused failures
^^^^^^^^^^^^^^^^^^^^^^

Symptoms:

* crashes only when a plugin is configured
* non-deterministic behavior across runs with the same inputs

Start here:

* ``--plugin-activity-file=<file>`` to capture plugin activity
* ``--no-default-plugins`` to isolate
* Crash handler output from ``GnuLdDriver::defaultSignalHandler(...)`` can
  explicitly call out a plugin as the likely crash source

Output emission failures
^^^^^^^^^^^^^^^^^^^^^^^^

Symptoms:

* "unwritable output" / commit errors
* output size verification failures

Start here:

* ``Linker::emit()`` in ``lib/Core/Linker.cpp`` (``llvm::FileOutputBuffer`` creation/commit)


Practical debugging checklist
-----------------------------

When a link fails and you need actionable data quickly, try (in order):

1. Add ``--reproduce-on-fail=default`` (or ``--reproduce=default``) to get a
   ``<output>.tar`` tarball automatically; supply an explicit filename if needed.
2. Add ``--verbose --trace=command-line --trace=files``.
3. Enable map output: ``-M --Map=layout.map --MapStyle=Text`` (or YAML).
4. If plugins are involved: ``--plugin-activity-file=plugins.json`` and try
   ``--no-default-plugins`` to isolate.
5. If time-sensitive or flaky: ``--print-timing-stats`` and consider
   ``--emit-timing-stats=<file>`` to capture timing consistently.


Debugging runtime crashes in ELD-built images
---------------------------------------------

This section is for cases where the link *succeeds* but the produced ELF image
(executable / shared library / firmware image) fails at runtime (crash, abort,
unexpected exception, bad unwind/backtrace, etc.).

Preserve the right artifacts
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

For runtime debugging, the most common blocker is having only a stripped image
with no symbols or line tables.

Keep (or be able to re-create) at least:

* The exact linked ELF that ran (same build-id if you use build-ids).
* An unstripped ELF (or a separate ``.debug`` file) that matches the runtime
  image.
* The ELD map file (``--Map=... --MapStyle=Text`` or YAML) for fast
  address-to-section/symbol correlation.
* The crash report: PC/LR/SP, full backtrace if available, and a core dump when
  possible.

If your production image must be stripped, keep debug info out-of-band using
``llvm-objcopy`` (or GNU ``objcopy``):

.. code-block:: sh

   llvm-objcopy --only-keep-debug app app.debug
   llvm-objcopy --strip-debug --strip-unneeded app app.stripped
   llvm-objcopy --add-gnu-debuglink=app.debug app.stripped

Use linker map files for layout correlation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When debugging runtime failures that are layout-sensitive (for example: wrong
PLT/GOT access, unexpected text/rodata placement, thunk/trampoline differences,
RELRO placement), generate and keep a linker map file for the exact link:

.. code-block:: sh

   eld ... -M --Map=layout.map --MapStyle=Text

The map file is often the fastest way to answer: "which output section/segment
contains this address?" and "why did this archive member/section get pulled in?".

Symbolize a crash address (PC) quickly
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you have an address from a crash report (for example ``PC=0x...``) you can
usually get file:line without opening a debugger:

.. code-block:: sh

   # Pick one:
   llvm-addr2line -f -C -e ./app 0xADDR
   addr2line      -f -C -e ./app 0xADDR

For PIE executables and shared libraries under ASLR, ``0xADDR`` is typically a
*runtime virtual address*. Convert it to an ELF-relative address first:

1. Find the module load base (``/proc/<pid>/maps`` for a running process, or
   ``image list -o -f`` in lldb for a core).
2. Compute ``REL = ADDR - BASE``.
3. Run ``addr2line`` on ``REL`` using the corresponding module file.

If you are using sanitizers, make sure symbolization is enabled and points at a
working symbolizer:

* ``LLVM_SYMBOLIZER_PATH=/path/to/llvm-symbolizer``
* ``ASAN_OPTIONS=symbolize=1:abort_on_error=1`` (plus your project defaults)
* ``UBSAN_OPTIONS=print_stacktrace=1``

Debug with lldb (core dumps and live debugging)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

For a core dump:

.. code-block:: sh

   ulimit -c unlimited
   ./app   # reproduce crash
   lldb -c core ./app

On systems using ``systemd-coredump``, ``coredumpctl`` is often the easiest:

.. code-block:: sh

   coredumpctl list ./app
   coredumpctl dump <PID> --output=core
   lldb -c core ./app

In lldb, start with:

* ``bt`` / ``thread backtrace all``
* ``register read`` and ``disassemble -m -p --start-address $pc``
* ``image list -o -f`` (verify loaded modules + load addresses)

If you cannot run the image locally (cross/embedded), use ``lldb-server`` on the
target and attach from the host toolchain debugger.

Run musl builds under qemu (quick cross-runtime triage)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you need a fast, reproducible runtime environment (especially for
cross-target issues), a practical workflow is: build a small musl-based binary
and run it under qemu (user-mode or system-mode).

User-mode qemu (recommended for simple tests)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For Linux user-mode emulation, run the target executable directly:

.. code-block:: sh

   # Examples (pick your target qemu):
   qemu-aarch64 ./app
   qemu-arm     ./app
   qemu-riscv64 ./app
   qemu-riscv32 ./app
   qemu-x86_64  ./app

If the binary is dynamically linked, point qemu at a sysroot that contains the
target loader + shared libraries (musl or glibc as appropriate):

.. code-block:: sh

   qemu-aarch64 -L /path/to/<triplet>/sysroot ./app

Common qemu-user tips:

* ``-strace`` prints syscalls (useful when a crash is actually an ``ENOENT`` /
  loader issue).
* ``-E VAR=...`` sets environment variables for the emulated program (for
  example ``-E LD_LIBRARY_PATH=...``).
* ``-d in_asm,cpu,exec -D qemu.log`` logs executed instructions; it is noisy but
  can pinpoint the last instruction before a crash.
* ``-g <port>`` enables a gdbstub; you can attach with lldb using gdb-remote:

  .. code-block:: sh

     qemu-aarch64 -g 1234 ./app
     lldb ./app
     (lldb) gdb-remote 1234

System-mode qemu (when you need a full OS image)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Use system-mode (``qemu-system-*``) when user-mode is insufficient (for example:
kernel/driver interactions, missing syscalls, or you need a full rootfs).

In system-mode, typical debugging flags include:

* ``-s -S`` (open gdbstub and stop at reset)
* ``-d in_asm,cpu,exec -D qemu.log`` (instruction logging; very verbose)

Inspect exception handling and unwinding
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Runtime failures that look like "crash while unwinding", "terminate called after
throwing", or incorrect backtraces typically reduce to missing/mismatched unwind
or exception tables.

At link time, ELD may merge/synthesize unwind-related sections (for example
``.eh_frame`` and ``.eh_frame_hdr``) and can also process SFrame
(``.sframe`` with ``--sframe-hdr``). For ARM EHABI you may also see
``.ARM.exidx`` / ``.ARM.extab``.

Quick checks:

.. code-block:: sh

   llvm-readelf -S ./app | grep -E \"eh_frame|eh_frame_hdr|gcc_except_table|ARM\\.exidx|ARM\\.extab|sframe\"
   llvm-readelf --unwind ./app   # unwind info (includes .eh_frame when present)

Common causes of missing/insufficient unwind info:

* Built without unwind tables (toolchain flags such as ``-fno-asynchronous-unwind-tables``,
  ``-fno-unwind-tables``).
* Over-aggressive stripping (for example link-time ``--strip-debug``) combined
  with not keeping a matching ``.debug`` file.
* Inconsistent binaries/libraries at runtime (debugging with one ELF but running
  a different one; mismatched build-ids).
* For C++ exceptions: missing runtime pieces (for example ``__gxx_personality_v0``
  not resolved, or the wrong unwinder/libgcc_s on the target).

If the problem is "backtrace is garbage" rather than exceptions specifically,
also consider building with frame pointers (for example ``-fno-omit-frame-pointer``)
and validating that your unwinder matches the format your toolchain emits
(``.eh_frame`` vs SFrame vs target-specific unwind tables).

Inspect loader and shared-library problems
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Crashes very early in process startup (before ``main``) are often due to dynamic
loader configuration rather than "code bugs".

.. code-block:: sh

   llvm-readelf -l ./app    # interpreter (PT_INTERP) and program headers
   llvm-readelf -d ./app    # NEEDED, RPATH/RUNPATH
   ldd ./app           # what the system thinks will load (when available)

For glibc-based systems, ``LD_DEBUG`` can explain loader decisions:

.. code-block:: sh

   LD_DEBUG=libs,bindings ./app

Target ABI / relocations / GOT-PLT debugging notes
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When debugging runtime crashes that involve dynamic linking, relocation
application, or address materialization sequences, it helps to look at:

* Relocations: ``llvm-readelf -r ./app`` and ``llvm-readelf --dyn-relocations ./app``.
* Dynamic table: ``llvm-readelf -d ./app`` (``NEEDED``, ``RPATH/RUNPATH``, flags).
* Program headers: ``llvm-readelf -l ./app`` (``PT_LOAD`` flags/alignment, ``PT_GNU_RELRO``).
* GOT/PLT-related sections and their contents:

  .. code-block:: sh

     llvm-readelf -S ./app | grep -E \"\\.plt|\\.got|\\.rela(\\.plt|\\.dyn)|\\.rel(\\.plt|\\.dyn)\"
     llvm-readelf -x .got ./app        2>/dev/null | less
     llvm-readelf -x .got.plt ./app    2>/dev/null | less
     llvm-readelf -x .plt ./app        2>/dev/null | less

  Correlate entries with disassembly + relocations:

	  .. code-block:: sh

	     llvm-objdump -dr --no-show-raw-insn ./app | less
	     llvm-readelf -r ./app | less

External ABI / ELF references (authoritative relocation tables)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When you need a definitive answer for relocation semantics, PLT/GOT conventions,
TLS models, or calling convention/stack rules, prefer the architecture psABI/ABI
documents (rather than reverse-engineering from a tool implementation).

Useful starting points:

* Generic ELF / System V ABI:

  * Linux Foundation reference index:
    `ELF and ABI Standards <https://refspecs.linuxfoundation.org/elf/index.html>`_
  * System V gABI Edition 4.1:
    `gabi41.pdf <https://refspecs.linuxfoundation.org/elf/gabi41.pdf>`_
  * ELF Object File Format (modern publication of the ELF chapters):
    `Xinuos ELF spec (HTML) <https://gabi.xinuos.com/v42/elf/00-foreword.html>`_

* x86_64 System V psABI:

  * psABI PDF:
    `x86_64-SysV-psABI.pdf <https://refspecs.linuxfoundation.org/elf/x86_64-SysV-psABI.pdf>`_
  * Sources/repo:
    `x86-64 psABI (GitLab) <https://gitlab.com/x86-psABIs/x86-64-ABI>`_

* Arm AArch32 / AArch64:

  * Official Arm ABI repository (AAELF32/AAELF64, PCS, EHABI, etc):
    `ARM-software/abi-aa <https://github.com/ARM-software/abi-aa>`_
  * AAELF (Arm ELF, AArch32):
    `IHI0044E_aaelf.pdf <https://developer.arm.com/documentation/ihi0044/e/IHI0044E_aaelf.pdf>`_
  * AAELF64 (AArch64 ELF):
    `IHI0056G_2020Q2_aaelf64.pdf <https://github.com/ARM-software/abi-aa/blob/main/legacy-documents/aaelf64/ihi0056_G/IHI0056G_2020Q2_aaelf64.pdf>`_

* RISC-V psABI:

  * Spec site:
    `riscv-elf-psabi-doc <https://riscv-non-isa.github.io/riscv-elf-psabi-doc/>`_
  * Sources/releases:
    `riscv-elf-psabi-doc (GitHub) <https://github.com/riscv-non-isa/riscv-elf-psabi-doc>`_

* Qualcomm Hexagon:

  * ABI user guide (includes Hexagon-specific ELF/relocations):
    `Qualcomm Hexagon ABI User Guide (PDF) <https://docs.qualcomm.com/bundle/publicresource/80-N2040-23_REV_K_Qualcomm_Hexagon_Application_Binary_Interface_User_Guide.pdf>`_

Architecture-specific relocation names are target-defined; a quick way to see
what you are dealing with is the relocation inventory command in this guide.
The following are *common* relocation families you may see during runtime triage:

* **x86_64 ABI**: look for ``R_X86_64_*`` (for example: ``*_RELATIVE``,
  ``*_JUMP_SLOT``, ``*_GLOB_DAT``, ``*_PC32``, ``*_PLT32``, ``*_GOTPCREL*``).
* **AArch64 ABI**: look for ``R_AARCH64_*`` (for example: ``*_CALL26``,
  ``*_JUMP26``, ``*_ADR_PREL_PG_HI21``, ``*_ADD_ABS_LO12_NC``, ``*_RELATIVE``,
  ``*_JUMP_SLOT``, ``*_GLOB_DAT``).
* **ARM ABI (arm/thumb)**: look for ``R_ARM_*`` (for example: ``*_CALL``,
  ``*_JUMP24``, ``*_THM_CALL``, ``*_THM_JUMP24``) and, on EHABI platforms,
  unwind tables like ``.ARM.exidx``/``.ARM.extab``.
* **RISC-V 32/64**: look for ``R_RISCV_*`` (for example: ``*_PCREL_HI20``,
  ``*_PCREL_LO12_*``, ``*_CALL*``, ``*_JAL``, plus dynamic relocations like
  ``*_RELATIVE``, ``*_JUMP_SLOT``, ``*_GLOB_DAT``).
* **Hexagon ABI**: look for ``R_HEX_*`` relocations; use ``llvm-readelf -r`` and
  disassembly to connect the relocation type to the instruction sequence.

ARM-specific: verifying veneers/thunks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On ARM/Thumb, the linker may need to create veneers/thunks (branch islands) when
branches cannot reach their targets or when interworking is required.

When a crash looks like a bad branch target or a call landing in the wrong mode:

* Enable and inspect trampoline/thunk diagnostics and maps:
  ``--trampoline-map`` (plus the usual text map file).
* Disassemble around the call site and look for a veneer sequence and its
  relocation(s): ``llvm-objdump -dr --start-address=... --stop-address=...``.
* Confirm the callee symbol type and interworking expectations in the symbol
  table (``llvm-readelf -s --extra-sym-info``).

Minimize runtime failures with A/B experiments
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When a runtime crash is hard to reason about, treat it like a minimization
problem: change one knob at a time until the failure becomes deterministic, then
pinpoint which library/object/relocation pattern is responsible.

Dynamic-linking knobs that often turn "mystery crashes" into actionable data:

* Force eager binding (converts lazy-PLT crashes into startup failures):

  .. code-block:: sh

     LD_BIND_NOW=1 ./app

  Or make it a link-time property of the binary: ``-Wl,-z,now``.

* Strengthen shared-library resolution rules (catch unresolved imports sooner):
  ``-Wl,-z,defs`` (or ``-Wl,--no-undefined`` depending on toolchain).

* Toggle dependency pruning and interposition-related behavior:
  ``-Wl,--as-needed`` / ``-Wl,--no-as-needed`` and (when applicable)
  ``-Wl,-Bsymbolic`` / ``-Wl,-Bsymbolic-functions``.

* Toggle RELRO (affects which GOT/relocation targets become read-only at
  runtime): ``-Wl,-z,relro`` and ``-Wl,-z,norelro``.

Use these knobs together with map files and relocation inspection (see below) to
connect "crash address" -> "instruction" -> "relocation" -> "symbol" -> "DSO
that provides it".

Identify which shared library caused a crash (swap-and-isolate)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If the crash only reproduces with a particular shared library present, isolate
it by swapping one dependency at a time:

1. Confirm what actually loads:

   .. code-block:: sh

      ldd ./app
      llvm-readelf -d ./app | less

2. Swap the suspected DSO:

   * Use ``LD_LIBRARY_PATH`` to point at an alternate build directory.
   * Use ``LD_PRELOAD`` to force a specific ``.so`` (for interposition or to
     override a weak/indirect dependency).
   * Rename/move one dependency to force a load failure; if the crash disappears
     when the DSO is absent (or replaced), that narrows the search quickly.

3. Ensure you are swapping *compatible* binaries:

   * Same target triple/ABI, same libc/loader expectations.
   * Prefer swapping libraries built by the same toolchain revision first
     (compiler + assembler + linker + runtimes).
   * Verify build-ids match what you intend to test:

     .. code-block:: sh

        llvm-readelf -n ./app | less
        llvm-readelf -n ./libfoo.so | less

If you suspect a linker-specific issue in a shared library, rebuild just that
library with an alternate linker (GNU ld / gold / lld, depending on target
support) and re-run the A/B test. If the crash tracks the linker choice with the
same sources/flags, it is strong evidence of a link-time layout/relocation
problem rather than a pure runtime logic bug.

Switching toolchains/compilers to bisect regressions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If the failure appeared after a toolchain update, a fast way to narrow root
cause is to bisect *one dimension at a time*:

* Swap compiler only (keep assembler/linker constant) to see if codegen changes
  are responsible.
* Swap linker only (keep compiler constant) to see if layout/relocation handling
  is responsible.
* Swap runtime libraries (libc, libstdc++/libc++, libgcc_s/libunwind) to catch
  ABI or unwinder differences.

Use build-ids, map files, and the relocation inventory command to keep these
experiments grounded in “what changed” rather than “what you think changed”.

Write small regression tests (symbols/relocs/dynamic flags)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When you can reproduce a runtime failure, try to turn it into a small *link-time
observable* property so it can be tested without running on a target.

In this repo, most linker tests are lit tests (``*.test``) that:

* compile tiny C/asm inputs,
* run ``%link`` (eld) with specific flags, and
* verify ELF properties using ``%readelf`` (``llvm-readelf``), ``llvm-readobj``,
  or ``%objdump`` + ``FileCheck``.

Examples of properties that correlate strongly with runtime behavior:

* Symbol kind/binding/visibility (from ``llvm-readelf -s --extra-sym-info`` / ``nm``).
* Relocation types and placement (from ``llvm-readelf -r`` / ``llvm-readobj -r``).
* Dynamic linking flags and segments (``llvm-readelf -d`` for ``BIND_NOW`` /
  ``FLAGS_1``, and ``llvm-readelf -l`` for ``PT_GNU_RELRO`` and ``p_align``).

If you need a starting point, see ``test/Templates/ExampleOfMyLitTest.test`` and
existing tests under ``test/lld/ELF`` that check relocations and flags like
``-z now`` via ``%readelf``/``%objdump``.

Illustrative lit pattern (dynamic flags + RELRO):

.. code-block:: text

   # RUN: %link %linkopts -shared -z now -z relro %t.o -o %t.so
   # RUN: %readelf -d %t.so | FileCheck %s --check-prefix=DYN
   # RUN: %readelf -l %t.so | FileCheck %s --check-prefix=PHDR
   # DYN: BIND_NOW
   # PHDR: GNU_RELRO

Other useful inspection tools
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

* Address/section correlation: ``llvm-nm -n``, ``nm -n``, ``llvm-objdump -d``,
  ``objdump -d``, and the ELD map file.
* ELF metadata: ``llvm-readelf -h -l -S -s -n`` (build-id in ``llvm-readelf -n``).
* Relocation inventory across a build tree (quickly see which relocation types
  are present in objects/archives):

  .. code-block:: sh

     find . -name \"*.o\" -o -name \"*.a\" | xargs llvm-readelf -r | awk '{print $3}' | sort -u | grep R_

  More robust (handles spaces in paths and avoids ``find`` precedence traps):

  .. code-block:: sh

     find . \\( -name \"*.o\" -o -name \"*.a\" \\) -print0 \\
       | xargs -0 llvm-readelf -r \\
       | awk '{print $3}' \\
       | sort -u \\
       | grep R_

* Inspect relocations around a crash site (when investigating wrong codegen,
  bad PLT/GOT usage, or runtime loader fixups):

  .. code-block:: sh

     # Disassemble with relocations shown (pick one):
     llvm-objdump -dr --no-show-raw-insn ./app | less
     objdump     -dr --no-show-raw-insn ./app | less

     # Narrow to a region around an address (adjust for PIE/ASLR first):
     llvm-objdump -dr --start-address=0xSTART --stop-address=0xSTOP ./app

* Segment properties / page alignment / RELRO (loader-relevant):

  .. code-block:: sh

     llvm-readelf -l ./app    # program headers: p_flags, p_align, PT_LOAD, PT_GNU_RELRO, PT_INTERP
     llvm-readelf -d ./app    # dynamic tags: DT_BIND_NOW, (FLAGS / FLAGS_1), RPATH/RUNPATH

* System-call tracing: ``strace -f`` (and ``ltrace`` when applicable).
* Memory corruption: ASan/UBSan/TSan builds; ``valgrind`` when supported.
