LTO Support
===========

.. contents::
   :local:

Overview
--------
ELD supports Link Time Optimization (LTO) using the LLVM LTO libraries. The
linker can consume LLVM bitcode directly or use embedded bitcode inside ELF
objects and drive Full LTO or ThinLTO code generation. This chapter describes
the LTO model used by ELD and the linker switches that control it.

Background: LLVM LTO
--------------------
LLVM LTO performs intermodular (whole program) optimization at link time.
Compared to traditional per-object optimization, LTO enables cross-module
inlining, dead code elimination, and whole-program analysis by merging or
summarizing LLVM IR across inputs. The LLVM LTO design emphasizes transparency
in the build system: compile with LTO enabled and link with an LTO-capable
linker, and the linker handles optimization without special build steps.

ThinLTO is LLVM's scalable LTO mode. It uses a summary index to avoid a full
merge of all IR, supports parallel code generation, and allows caching of
imported summaries or produced objects to speed up incremental builds. ELD
provides knobs for ThinLTO parallelism and caching, alongside Full LTO support
when modules are compiled for full LTO.

ELD LTO model
-------------
Inputs
~~~~~~
ELD triggers LTO when it sees bitcode inputs or when it is instructed to use
embedded bitcode sections in ELF files:

* **Bitcode inputs**: `.bc` inputs are always treated as LTO inputs.
* **Embedded bitcode**: ELD recognizes the `.llvmbc` section in ELF objects and
  can replace the native object with the embedded bitcode when LTO is enabled.
  This supports "fat" objects that carry both native code and LLVM bitcode.

Selecting LTO inputs
~~~~~~~~~~~~~~~~~~~~
LTO is enabled in two ways:

* `-flto` enables LTO if any bitcode input or embedded bitcode is present.
* `--include-lto-filelist` can be used without `-flto` to select which
  ELF inputs with embedded bitcode should be upgraded to LTO inputs.

When `-flto` is used, embedded bitcode is used by default unless excluded using
`--exclude-lto-filelist`. When `-flto` is not used, only file patterns listed
in `--include-lto-filelist` are upgraded to LTO inputs.

File lists accept one glob pattern per line (wildcards `*`, `?`, and `[]` are
supported). Patterns are matched against the input path as seen by the linker.

LTO phases in ELD
~~~~~~~~~~~~~~~~~
ELD performs LTO in distinct phases:

1. **Pre-LTO**: Inputs are read, symbols are collected, and linker scripts are
   evaluated. Bitcode inputs are registered for LTO.
2. **LTO**: LLVM performs symbol resolution across bitcode modules and emits
   native object files (Full LTO) or partitioned outputs (ThinLTO).
3. **Post-LTO**: The generated objects are inserted back into the link and
   linked like normal ELF objects. Map files can include pre-LTO and post-LTO
   details.

Linker scripts and LTO
~~~~~~~~~~~~~~~~~~~~~~
ELD supports LTO with linker scripts. When a script uses `SECTIONS` ordering,
ELD preserves input order for LTO-generated sections. If you need to disable
this ordering for performance or to match non-script ordering, use
`-flto-options=disable-linkorder`.

LTO switch reference
--------------------
This section lists the LTO-related switches supported by ELD. Some switches
also accept `--plugin-opt=...` aliases for compatibility with LLVMgold and lld.

Core LTO enablement and input selection
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* `-flto` or `--flto`
  Enable LTO if a bitcode file or embedded bitcode section is present.
* `--include-lto-filelist=<list>`
  Use this list to select which ELF inputs with embedded bitcode should be
  used for LTO when `-flto` is not present.
* `--exclude-lto-filelist=<list>`
  Use this list to exclude embedded bitcode inputs when `-flto` is present.

LTO output control and artifacts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* `--save-temps`
  Save intermediate LTO artifacts. Temporary files use the prefix
  `<output>.llvm-lto.*`.
* `--save-temps=<dir>`
  Save LTO temporary files under the specified directory.
* `--lto-emit-asm`
  Run LTO and emit assembly only. No final link is performed. Output files use
  the prefix `<output>.llvm-lto.<task>.s`.
  Alias: `--plugin-opt=emit-asm`.
* `--lto-emit-llvm`
  Run LTO and emit LLVM bitcode to the final output file (`-o`). No final link
  is performed.
  Alias: `--plugin-opt=emit-llvm`.
* `--lto-obj-path=<prefix>`
  Prefix for LTO-generated object files. When provided, output objects are not
  deleted after LTO.
  Alias: `--plugin-opt=obj-path=`.
* `-flto-options=lto-asm-file=<file[,file2,...]>`
  Use pre-generated LTO assembly files as inputs to the external assembler.
* `-flto-options=lto-output-file=<file[,file2,...]>`
  Use pre-generated LTO object files as outputs (bypasses LTO code generation).

LTO optimization control
~~~~~~~~~~~~~~~~~~~~~~~~
* `--lto-O=<level>`
  Set the LTO optimization level (0-4).
  Alias: `--plugin-opt=O`.
* `--lto-partitions=<number>`
  Set the number of code generation partitions.
  Alias: `--plugin-opt=lto-partitions=`.
* `--thinlto-jobs=<number>`
  Set the number of ThinLTO backend jobs. Overrides `--threads=` for ThinLTO.
  Alias: `--plugin-opt=jobs=`.
* `--lto-sample-profile=<file>`
  Provide a sample PGO profile for LTO.
  Alias: `--plugin-opt=sample-profile=`.
* `--lto-cs-profile-generate`
  Enable context-sensitive PGO instrumentation during LTO.
  Alias: `--plugin-opt=cs-profile-generate`.
* `--lto-cs-profile-file=<file>`
  Provide a context-sensitive profile for LTO.
  Alias: `--plugin-opt=cs-profile-path=`.
* `--lto-debug-pass-manager`
  Enable debug output for the new pass manager.
  Alias: `--plugin-opt=debug-pass-manager`.
* `--disable-verify`
  Disable the LLVM IR verifier in the LTO pipeline.
  Alias: `--plugin-opt=disable-verify`.
* `--dwodir=<dir>`
  Directory for split DWARF `.dwo` files generated during LTO.

LLVM option passthrough
~~~~~~~~~~~~~~~~~~~~~~~
* `--plugin-opt=-<llvm-option>`
  Pass a raw LLVM option to LTO (LLVMgold compatibility). For example,
  `--plugin-opt=-debug-pass-manager`.
* `-flto-options=codegen=<llvm-arg>`
  Pass LLVM codegen options to LTO (supports `-O`, `-mcpu=`, `-mattr=`, and
  arbitrary LLVM options).
* `-flto-options=<string>`
  Passes additional LTO plugin options through. For example, tests use
  `-flto-options=no-merge-modules` and `-flto-options=codegen=-split-lto-cg`.

Assembler control
~~~~~~~~~~~~~~~~~
* `-flto-use-as`
  Use the external assembler instead of the integrated assembler for LTO.
* `-flto-options=asmopts=<arg>`
  Extra arguments passed to the external assembler during LTO.

LTO caching
~~~~~~~~~~~
* `-flto-options=cache`
  Enable ThinLTO caching. By default, cache output is written to
  `<output>.ltocache`.
* `-flto-options=cache=<dir>`
  Enable ThinLTO caching using the specified directory.

Symbol preservation and ordering
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* `-flto-options=preserveall`
  Preserve all bitcode symbols during LTO.
* `-flto-options=preserve-sym=<sym1,sym2,...>`
  Preserve the specified symbols in LTO.
* `-flto-options=preserve-file=<file>`
  Preserve the symbols listed in the given file (one symbol per line).
* `-flto-options=disable-linkorder`
  Disable link order enforcement when linker scripts are present.
* `-flto-options=verbose`
  Enable verbose LTO diagnostics and trace output.

Diagnostics and tracing
~~~~~~~~~~~~~~~~~~~~~~~
* `--trace-lto`
  Trace LTO stages and emit additional diagnostics.
* `--opt-record-file`
  Emit LTO optimization remarks. ELD writes `-LTO.opt.yaml` alongside the
  output file.
* `--display-hotness`
  Display hotness information with optimization remarks.
* `--print-timing-stats`, `--emit-timing-stats`
  Emit or print timing stats, including LTO phases.

Examples
--------
Full LTO with embedded bitcode:
::

  clang -flto -c foo.c -o foo.o
  clang -flto -c bar.c -o bar.o
  ld.eld -flto foo.o bar.o -o app.elf

ThinLTO with parallel backends and cache:
::

  clang -flto=thin -c foo.c -o foo.o
  clang -flto=thin -c bar.c -o bar.o
  ld.eld -flto --thinlto-jobs=8 --flto-options=cache=thinlto.cache \
    foo.o bar.o -o app.elf

Selective LTO on a subset of objects:
::

  # lto_list.txt contains glob patterns like:
  #   libfoo/*.o
  #   *vector_math*.o
  ld.eld --include-lto-filelist=lto_list.txt foo.o bar.o -o app.elf

Emit LTO assembly for inspection:
::

  ld.eld -flto --lto-emit-asm foo.o bar.o -o app.elf
  # Output files: app.elf.llvm-lto.0.s, app.elf.llvm-lto.1.s, ...

References
----------
* LLVM Link Time Optimization: Design and Implementation
  (`https://llvm.org/docs/LinkTimeOptimization.html`)
