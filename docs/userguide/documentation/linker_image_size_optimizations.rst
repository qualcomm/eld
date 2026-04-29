Linker Image Size Optimizations
===============================

.. contents::
   :local:

Overview
--------
This chapter focuses on image-size reduction techniques that are controlled at
link time in ELD. In practice, best results come from using compiler and linker
options together:

* Compile with section granularity so the linker can drop unused code/data.
* Enable linker optimizations that deduplicate or discard content.
* Validate size deltas using map files and ELF section/symbol inspection.

Recommended baseline flow
-------------------------
For size-sensitive builds, start with:

1. Compile with ``-ffunction-sections -fdata-sections``.
2. Link with ``--gc-sections``.
3. Use ``--print-gc-sections`` during tuning to confirm what is removed.
4. Check output with ``readelf -S/-s`` and ``-Map`` reports.

Section garbage collection
--------------------------
``--gc-sections`` removes input sections that are not reachable from retained
roots (entry, undefined symbols requested on the command line, and other
link-relevant roots). This is usually the single highest-impact size option for
large applications and firmware images.

Useful companion options:

* ``--print-gc-sections``: print what got collected.
* ``--no-gc-sections``: disable GC (for bring-up/comparison).

Example:
::

  clang -c a.c -o a.o -ffunction-sections -fdata-sections
  clang -c b.c -o b.o -ffunction-sections -fdata-sections
  ld.eld --gc-sections --print-gc-sections a.o b.o -o app.elf

String and constant merging controls
------------------------------------
ELD performs merge optimizations for mergeable data. These typically reduce
``.rodata`` size when payloads are duplicated across translation units.

What merge-strings does
^^^^^^^^^^^^^^^^^^^^^^^
String merging operates on mergeable string input sections (typically sections
with ``SHF_MERGE|SHF_STRINGS`` flags such as ``.rodata.str*``).

At a high level, ELD:

* builds mergeable string fragments from input sections,
* picks canonical string storage when contents are identical, and
* retargets merge-kind relocations to the canonical string location.

This reduces duplicated string bytes in the final image while preserving
semantic references.

What merge-data (merge-constants) does
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Constant-data merging operates on non-string mergeable constant sections
(for example, ``.rodata.cst*`` with ``SHF_MERGE``).

At a high level, ELD:

* identifies identical constant payloads (respecting merge semantics such as
  entry size/alignment constraints),
* keeps one canonical constant payload, and
* retargets eligible merge-kind relocations to that canonical payload.

This reduces duplicate constant bytes in ``.rodata`` and related mergeable
constant sections.

Map file view for merge optimizations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
In text map files, merged content is represented as a surviving canonical input
section entry plus comment lines for merged contributors.

Typical merged-constant pattern:
::

  .rodata.cst4   <out_off> <size> <file2.o> #SHT_PROGBITS,SHF_ALLOC|SHF_MERGE,4
    # .rodata.cst4 0x0 <file1.o>

Here, the non-comment line is the canonical contributor kept in layout order.
Comment-prefixed entries indicate inputs merged into that canonical storage.

For merge-strings, ``--MapDetail=show-strings`` adds string-content-oriented
details (for example, merged string contributors under the canonical line).
Use this when auditing why/where string payloads were coalesced.

For broader map-file structure and navigation details, see
:doc:`linker_map_files`.

How garbage collection affects merge data
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Garbage collection runs before merge optimizations in the normal final-link
flow. As a result:

* discarded input sections do not contribute merge-data/merge-string candidates,
* only live sections participate in canonicalization, and
* relocation retargeting is applied for live relocation paths, while discarded
  sections/relocations do not affect final merged placement.

Practical implication: enabling ``--gc-sections`` can reduce both direct code/
data size and secondary merged footprint, because dead contributors are removed
before merge-data and merge-strings canonicalization.

Available controls:

* ``--no-merge-strings``: disable string merging.
* ``--no-merge-constants``: disable mergeable constant-data canonicalization.

When to disable:

* Debugging layout/address-sensitive issues where deterministic one-to-one input
  retention is preferred over deduplication.
* A/B measurement to quantify the exact contribution of each merge pass.

Example A/B run:
::

  ld.eld foo.o bar.o -o app.default.elf
  ld.eld --no-merge-constants foo.o bar.o -o app.nomergeconst.elf
  readelf -S -W app.default.elf app.nomergeconst.elf

Strip and symbol-table reduction
--------------------------------
For release images, symbol/debug stripping can significantly reduce file size.

* ``--strip-debug`` (or ``-S``): drop debug information.
* ``--strip-all`` (or ``-s``): strip all symbols.
* ``--discard-all`` (or ``-x``): discard all local symbols.
* ``--discard-locals`` (or ``-X``): discard local temporary symbols.

Pick the least aggressive mode that still satisfies post-link tooling needs
(debuggers, profilers, crash pipelines, symbolizers).

Unwind data format choice
-------------------------
If your target and runtime support it, ``--sframe-hdr`` can reduce unwind
metadata size compared to traditional unwind metadata in some workloads.
Validate this per target by comparing section sizes before and after.

LTO for size-oriented builds
----------------------------
LTO can reduce final image size through cross-module dead stripping and
inlining decisions that are impossible in file-local compilation.

Typical knobs:

* ``-flto``: enable LTO when bitcode is available.
* ``--lto-O=<level>``: tune LTO optimization level.
* ``--lto-partitions=<N>`` / ``--thinlto-jobs=<N>``: tune ThinLTO scale.

For details, see :doc:`lto_support`.

Layout and segment-size considerations
--------------------------------------
Image file size and memory footprint are not always the same metric:

* File size depends on what bytes are materialized in file-backed segments.
* Memory size depends on load addresses, segment alignment, and NOBITS handling.

Some layout options (for example, page alignment and BSS conversion behavior)
can trade runtime/load behavior against output file size. See :doc:`layout` and
:doc:`linker_script` when tuning these constraints.

Measure and iterate
-------------------
A repeatable size-optimization loop:

1. Establish a baseline:
   ``ld.eld ... -Map baseline.map -o baseline.elf``
2. Enable one optimization at a time.
3. Compare:
   * ``readelf -S -W`` for section size deltas.
   * map-file totals and per-section contributors.
   * symbol-level changes via ``readelf -s -W``.
4. Keep only changes that improve size without breaking runtime/debug goals.

Practical profiles
------------------
Debug profile (size-aware, debuggable):

* ``--gc-sections``
* keep symbols/debug data
* optional merge pass disabling for diagnostics

Release profile (minimum file size focus):

* ``--gc-sections``
* default merge passes enabled
* ``--strip-debug`` or ``--strip-all`` (as allowed by deployment pipeline)
* optional LTO with tuned ``--lto-O``
