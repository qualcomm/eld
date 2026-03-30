# Changelog
All notable behavior changes in this repository are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), adapted to weekly buckets instead of release tags.

## Table of Contents

### Highlights
- [Release Markers](#release-markers)
- [Major Highlights](#major-highlights)

### 2026
- [March 2026](#march-2026)
- [February 2026](#february-2026)
- [January 2026](#january-2026)

### 2025
- [December 2025](#december-2025)
- [November 2025](#november-2025)
- [October 2025](#october-2025)
- [September 2025](#september-2025)
- [August 2025](#august-2025)
- [July 2025](#july-2025)
- [June 2025](#june-2025)
- [May 2025](#may-2025)
- [April 2025](#april-2025)
- [March 2025](#march-2025)

## Release Markers

- `release/22.x` (released 2026-03-14)

## Major Highlights

### 2026

- March: added `-z separate-loadable-segments`, fixed PIE handling for absolute `--defsym`, and improved AArch64 static TLS/IFunc support.
- February: added linker script `INSERT BEFORE/AFTER` and `NEXT_SECTION`, added `--start-lib/--end-lib`, and expanded LTO reporting options.
- January: expanded LTO CLI compatibility (`--thinlto-jobs`, `--lto-partitions`, `--lto-obj-path`) and improved layout convergence behavior.

### 2025

- November-December: major x86_64 dynamic-linking improvements (PLT/GOT/.rela.plt, TLSGD/TLSLD, IRELATIVE/ifunc), plus initial shared-library symbol versioning support.
- July-August: substantial linker-script and diagnostics hardening, plus RISC-V `TLSDESC` and `DriverFlavor` support.
- April-June: foundational feature ramp-up across emulations, plugin APIs, x86_64 static relocations, and linker-script expression handling.

## 2026

### March 2026

#### [2026-03-16] - 2026-03-16 to 2026-03-22
##### Changed
- Align start of tdata

#### [2026-03-09] - 2026-03-09 to 2026-03-15
##### Added
- Support -z separate-loadable-segments
- [hexagon] Add linux emulation
##### Changed
- Separate ctors/dtors and init_array/fini_array sections
##### Fixed
- Resolve --defsym absolute symbols at link time in PIE links

#### [2026-03-02] - 2026-03-02 to 2026-03-08
##### Added
- Improve IFunc support for AArch64 static executables
- [build] Add external llvm support
- [AArch64] Static TLS support

### February 2026

#### [2026-02-23] - 2026-02-23 to 2026-03-01
##### Added
- [LTO] Add --plugin-opt=stats-file= option
- [LTO] Add optimization remarks options
- Add support for parsing SORT(CONSTRUCTORS)
##### Changed
- [LTO] Move existing LTO option to the LTO group
##### Fixed
- Fix __eh_frame_* symbols
- [LinkerScript] fix sort constructors
- [aarch64] Fix TLS IE/GD support

#### [2026-02-16] - 2026-02-16 to 2026-02-22
##### Added
- [RISC-V] Add -no-relax-tlsdesc
- Add support for NEXT_SECTION
- Add --record-command-line extended linker option
##### Fixed
- Fix address of `_GLOBAL_OFFSET_TABLE_`

#### [2026-02-09] - 2026-02-09 to 2026-02-15
##### Added
- [LinkerScript] Add support for INSERT AFTER/BEFORE
- Add --start-lib/--end-lib command line option
- [AArch64] Add support for R_AARCH64_LD64_GOTPAGE_LO15
##### Fixed
- Fix incorrect NEEDED entry due to unneeded shared lib reference
- Correct sysroot-prepend conditions for script inputs
- Fix __tbss_offset calculation

#### [2026-02-02] - 2026-02-02 to 2026-02-08
##### Added
- Add padding in TLS section for alignment
- [AArch64] Add support for pointer authentication relocations
- Add Initializing state to getAllOutputSections link state check
##### Changed
- Read merge string sections in parallel
##### Fixed
- UndefinedBehaviorSanitizer: signed integer overflow issue runtime error: signed integer overflow: 2147483628 + 2048 cannot be represented in type 'int' fix
- Fix resolving script inputs path when sysroot is specified
- [LinkerScript] Fix >RAM AT>RAM placement after RAM AT>ROM
##### Removed
- Remove ENABLE_ELD_PLUGIN_SUPPORT and related conditionals

### January 2026

#### [2026-01-26] - 2026-01-26 to 2026-02-01
##### Added
- Add support for print cmd
- Add changes required for H2+Picolibc
- Add new linker script for H2+Picolibc
##### Changed
- Reiterate layout step until section addresses converge
- Improve help for previously added LTO options
- Default to Linux Emulation on RISC-V
##### Fixed
- Fix crash when AArch64 objects contain unsupported relocations.
- disable ASSERT macro in non-debug builds

#### [2026-01-19] - 2026-01-19 to 2026-01-25
##### Fixed
- Fix crash when image refer to certain kind of symbols
- Fix `FDEFragment::classof` function

#### [2026-01-12] - 2026-01-12 to 2026-01-18
##### Added
- Add --lto-obj-path option and its alias
##### Changed
- Handle NOLOAD/TBSS/PROGBITS sections
##### Fixed
- Fix ELDExpected windows failure

#### [2026-01-05] - 2026-01-05 to 2026-01-11
##### Added
- Add --plugin-opt=save-temps alias for --save-temps
- Add --thinlto-jobs= option and its alias --plugin-opt=jobs=
- Add lto-partitions= and --plugin-opt=lto-partitions= options
- Support `-z separate-code` / `-z noseparate-code`
##### Changed
- Update lib/Readers/ELFSection.cpp
- [RISCV] Use Vendor Reloc Names
##### Fixed
- Fix re-evaluation of DEFINED expression

## 2025

### December 2025

#### [2025-12-29] - 2025-12-29 to 2026-01-04
##### Added
- Add `-disable-verify` and its plugin-opt alias
- Add `--lto-cs-profile-generate` and `--lto-cs-profile-file=` options
##### Fixed
- Fix parsing of `"archive:mem"` input section description pattern

#### [2025-12-22] - 2025-12-22 to 2025-12-28
##### Fixed
- Fix build failure error undeclared CHERIOT1VendorRelocationOffset

#### [2025-12-08] - 2025-12-08 to 2025-12-14
##### Added
- Add `-debug-pass-manager` option for LTO (#662)

#### [2025-12-01] - 2025-12-01 to 2025-12-07
##### Added
- Add initial support for creating shared library with symbol versioning
- Add eld tablegen targets to LLVM_COMMON_DEPENDS
##### Changed
- [ELFSection] Use sentinel value instead of optional
- [ArchiveFile] Store symbols by value and avoid storing strings.
- [ELFSection] Move DependentSections to side table
##### Fixed
- Relax static assert in `ELFSection.h`

### November 2025

#### [2025-11-24] - 2025-11-24 to 2025-11-30
##### Added
- [ELD][x86-64] Implement IRELATIVE relocations for ifunc support
- [x86_64] Add support for R_X86_64_TLSLD relocation
- [x86_64] Add support for R_X86_64_TLSGD relocation
##### Changed
- Evaluate output section end symbol assignments in partial link
- Move PAddr from ELFSection to OutputSectionEntry
- [x86-64] Copy relocations for absolutely referenced data symbols
##### Fixed
- Fix which function to handle spaces in executable paths on Windows
##### Removed
- [ELFSection] Remove unnecessary Annotations member.

#### [2025-11-17] - 2025-11-17 to 2025-11-23
##### Added
- Add release artifact builder
- [x86_64] Add R_X86_64_64 dynamic linking handling
##### Fixed
- [Relocator] Emit valid ranges in overflow diags (#599)
- Fix crash due to missing thin archive member
- Fix invalid 'Referenced Chunk ... deleted' error when printing map-file

#### [2025-11-10] - 2025-11-10 to 2025-11-16
##### Added
- Add primitive support for plugin activity log file functionality
- Support signed 64 bit values in diagnostics.
- Add guide that describes linker support for backwards compatibility
##### Changed
- const GeneralOptions
##### Fixed
- Report unbalanced chunk diag even if plugin fails in CreatingSections
- Fix FileCheck pattern for AbsoluteSymbolRelocation
- Fix YAML package dependecy and search path related windows failures

#### [2025-11-03] - 2025-11-03 to 2025-11-09
##### Added
- x86_64: Implement data call support for dynamic linking
##### Fixed
- Fix symbol resolution of EXTERN command symbols
- Fix crash on printing change out sect plugin op with invalid out sect

### October 2025

#### [2025-10-27] - 2025-10-27 to 2025-11-02
##### Added
- x86_64: Implement R_X86_64_PLT32 handling for dynamic linking scenarios
- x86_64: Add .rela.plt section creation for dynamic linking
- x86_64: Implement GOTPLTN initialization
##### Fixed
- x86_64: Fix PLTN stub instructions
- Fix classof usage for InputAction subclasses

#### [2025-10-20] - 2025-10-20 to 2025-10-26
##### Added
- [Hexagon] V91 support
##### Changed
- Update symbols with retain attribute
- improve memory usage diagnostics
##### Fixed
- [RISCV] Fix JAL overflow check

#### [2025-10-13] - 2025-10-13 to 2025-10-19
##### Added
- Add Memory and RegionAlias commands in eld::plugin::Script
- x86_64: Enable creation of .dynamic section
- Add support for --push-state / --pop-state functionality
##### Changed
- Eval AFTER_SECTIONS assignments after assignments within SECTIONS cmd
- Re-evaluate OUTSIDE_SECTIONS assignments whenever layout resets
- Divide OUTSIDE_SECTIONS into BEFORE_SECTIONS and AFTER_SECTIONS
##### Fixed
- x86_64: Fix PLT0 stub instructions to reference GOTPLT[1] and GOTPLT[2]
- x86_64: Fix GOTPLT0 to populate .dynamic address correctly

#### [2025-10-06] - 2025-10-06 to 2025-10-12
##### Added
- x86_64: Add support for R_X86_64_DTPOFF32 and R_X86_64_DTPOFF32 relocations
- Add missing cstdint includes
##### Fixed
- Fix NOCROSSREFS feature to work when there is no SECTIONS command
- fix break with llvm tip

### September 2025

#### [2025-09-29] - 2025-09-29 to 2025-10-05
##### Changed
- Emit text map-file even when the link crashes

#### [2025-09-22] - 2025-09-22 to 2025-09-28
##### Added
- Add ARM, AArch64, RISCV
##### Changed
- Define __eh_frame_* symbols as standard symbols

#### [2025-09-15] - 2025-09-15 to 2025-09-21
##### Added
- Add __eh_frame_hdr_start/end symbols in template linker script
- Add support for displaying plugin stats in map file
##### Fixed
- Fix -1 e_flags with binary inputs
- Fix missed code changes for linker detection of non contiguous tls sections
- Fix linker detection of non contiguous tls sections

#### [2025-09-08] - 2025-09-08 to 2025-09-14
##### Fixed
- Fix YamlLayoutPrinter crash when the link contains an empty archive

#### [2025-09-01] - 2025-09-01 to 2025-09-07
##### Added
- add reverse iterator github action
##### Fixed
- Fix incorrect padding value when both fill expr and command are used
- Fix DiagnosticEngine deadlock that can happen when the link crashes
- Fix section hash computation in LW::doesRuleMatchWithSection

### August 2025

#### [2025-08-25] - 2025-08-25 to 2025-08-31
##### Fixed
- Fix fragment padding size computation when output section is unaligned

#### [2025-08-18] - 2025-08-18 to 2025-08-24
##### Added
- x86_64: Add TLS Local Exec relocations support
- Add InputFile PluginAPIs
##### Changed
- Parse MEMORY command expressions in LexState::Expr
##### Fixed
- Fix UnaryOperator expression name

#### [2025-08-11] - 2025-08-11 to 2025-08-17
##### Added
- Fixup: Add %opt
- LTO: Support --lto-sample-profile=
##### Changed
- [RISC-V] Sort relocations
- Driver changes to inferred arch from program name/emulation

#### [2025-08-04] - 2025-08-04 to 2025-08-10
##### Added
- Support DriverFlavor
##### Changed
- Set emulation mode based on input files
- Return 0 instead of asserting if expression does not have result
- Skip invalid ASCII chars when parsing linker script
##### Fixed
- Fix assertion failure due to invalid glob pattern in linker script
- Fix PROVIDE feature for output section prologue expressions
- [ARM][AArch64] Fix computation of local-exec TLS relocations

### July 2025

#### [2025-07-28] - 2025-07-28 to 2025-08-03
##### Added
- [RISC-V] Add support for TLSDESC
- Add support for section annotations in map file
- Add a plugin API to add files to reproduce tarball
##### Changed
- [RISCV] Handle VENDOR Relocations with emit-relocs
##### Fixed
- [LinkerScript] Fix parsing of invalid file and section patterns

#### [2025-07-21] - 2025-07-21 to 2025-07-27
##### Added
- Support ALIGN_WITH_INPUT output section attribute
##### Changed
- Improve GNU-compatibility of the linker script parser
##### Fixed
- Fix reproduce crash when input is passed through linkerscript
- [LinkerScript] Fix fill padding for certain sections.
- Fix incorrect evaluation of ternary expression during GC

#### [2025-07-14] - 2025-07-14 to 2025-07-20
##### Added
- add sanitizer build
- Support PIE with TLS
##### Changed
- set alignment for loadable and non loadable segments
- empty segment warning should be delayed to postLayout
- annotate not provided symbols in the map file
##### Fixed
- Fix crash when invalid section is used with 'ALIGNOF' linker script command
- Fix parsing of ':ALIGN(...)' in linker script
- Fix mapping file feature for findConfigFile and getFileContents idiom

#### [2025-07-07] - 2025-07-07 to 2025-07-13
##### Added
- Add support for bitwise xor operator in linkerscript.
- Enable reproduce feature for findConfigFile + getFileContents use idiom
- Add both ternary branch expressions symbols into symbol resolver
##### Changed
- Initialize Expression::result as an empty optional object
- Emit empty segment warning only if linker-script warnings are enabled
##### Fixed
- Update checkout step to dynamicall pull the PR branch from the correct source
- Fix dot file malformed render.
- Fix DEFINED(...) function evaluation for undef but referenced symbols

### June 2025

#### [2025-06-30] - 2025-06-30 to 2025-07-06
##### Fixed
- Report undef symbol reference error for ALIGN linker script expression
- fix alignment for segments
- Fix crash on parsing ':ALIGN' in linker script

#### [2025-06-23] - 2025-06-23 to 2025-06-29
##### Added
- Add support for GOTPCREL and related relocations for static linking
- Add R_X86_64_PLT32 static relocation support
- [RISCV] Implement Relaxations for QC.E.LI/QC.LI
##### Changed
- Correctly set hasOutputSection property of output section plugin cmd
- Switch to an lld-compatible option instead of -codegen=
##### Fixed
- Fix x86_64 default image base
- Fix DT_NEEDED entry for shared libs without SO name
- Fix incorrect call to `Linker::emit()` with `-emit-timing-stats-in-output` flag

#### [2025-06-16] - 2025-06-16 to 2025-06-22
##### Added
- Add output and input section plugin framework APIs
- Add InputSectionSpec and LinkerScript plugin framework APIs
- Add ternary condition expression symbol as undefined reference symbol
##### Changed
- Return true when symbol table section is missing
##### Fixed
- Add -W[no-]error option to convert warnings into errors
- [RISCV] Fix ANDES Vendor Issue

#### [2025-06-09] - 2025-06-09 to 2025-06-15
##### Added
- Add symbols referred by shared libs to dynamic symbol table
- Add `getLinkerVersion` LinkerWrapper API
##### Changed
- silence symbol overrides

#### [2025-06-02] - 2025-06-02 to 2025-06-08
##### Added
- Add support for x86_64 emulation options
- Add a warning for empty segments
- Add warn diagnostics for zero-sized memory regions

### May 2025

#### [2025-05-26] - 2025-05-26 to 2025-06-01
##### Added
- Add `ELD_REGISTER_PLUGIN` macro

#### [2025-05-19] - 2025-05-19 to 2025-05-25
##### Added
- support omagic
##### Changed
- local symbols that need GOT slot require dynamic relocation
##### Fixed
- fix dllexport

#### [2025-05-12] - 2025-05-12 to 2025-05-18
##### Added
- [RISCV] Implement Relaxations for QC.E.J/QC.E.JAL
- [risc-v] Add --no-relax-zero option
##### Changed
- Make dynamic string table simpler to use
##### Fixed
- Fix dot counter handling for non-alloc sections

#### [2025-05-05] - 2025-05-05 to 2025-05-11
##### Added
- Support 'archive: member' file-pattern in input section description
##### Changed
- [LinkerScript] Parse symbol assignment inside PROVIDE in LexState::Expr

### April 2025

#### [2025-04-21] - 2025-04-21 to 2025-04-27
##### Added
- Add emulation option support for hexagon, arm and aarch64
- [LinkerScript] Add support for ALIGN_WITH_INPUT
##### Removed
- Explicitly disable copying/moving GeneralOptions and export GnuLdDriver

#### [2025-04-14] - 2025-04-14 to 2025-04-20
##### Added
- Add exidx_start and exidx_end symbols with PROVIDE_HIDDEN semantics
- Add FAQ on linker script changes required for TLS functionality
- Allow --patch-enable and local symbol stripping (#27)
##### Changed
- assignments/defsym needs to be processed in link order
- [riscv] More cases of using PLT address (#41)
##### Fixed
- Fix generation of duplicate DT_NEEDED entries
##### Removed
- [RISC-V] Disable default attribute mix warnings

#### [2025-04-07] - 2025-04-07 to 2025-04-13
##### Added
- Support building eld when BUILD_SHARED_LIBS=On
- Add primitive support of emulation options with ld.eld
- Add delimiter to "cannot read" diagnostic
##### Changed
- Change sort-section to permit space-delim
##### Fixed
- fix name of RISC-V extension

### March 2025

#### [2025-03-31] - 2025-03-31 to 2025-04-06
##### Added
- [arm] Support --target2=abs/rel/got-rel

#### [2025-03-10] - 2025-03-10 to 2025-03-16
##### Added
- Initial commit.
- Open-sourced the `eld` linker repository.
