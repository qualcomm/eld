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

* ``--reproduce <tarfilename>``: always produce a tarball
* ``--reproduce-compressed <tarfilename>``: compressed tarball
* ``--reproduce-on-fail <tarfilename>``: only on failure
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
* Writes a temporary ``.sh`` script that appends ``--reproduce build.tar`` to
  the command line and instructs the user to rerun.

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

1. Add ``--reproduce-on-fail repro.tar`` (or ``--reproduce repro.tar``).
2. Add ``--verbose --trace=command-line --trace=files``.
3. Enable map output: ``-M --Map=layout.map --MapStyle=Text`` (or YAML).
4. If plugins are involved: ``--plugin-activity-file=plugins.json`` and try
   ``--no-default-plugins`` to isolate.
5. If time-sensitive or flaky: ``--print-timing-stats`` and consider
   ``--emit-timing-stats=<file>`` to capture timing consistently.
