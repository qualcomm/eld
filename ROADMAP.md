# ELD Roadmap – Key Focus Areas

> **ELD** aims to be a production‑quality, open‑source linker for embedded systems, aligned with the LLVM ecosystem and the broader open‑source community.

---

## Table of Contents
- [Areas for Development](#areas-for-development)
- [Competitive Readiness and Usability](#competitive-readiness-and-usability)
- [Building Community Confidence](#building-community-confidence)
- [Legend](#legend)
- [Command Line Support (Roadmap)](#command-line-support-roadmap)
  - [Page and Memory Layout](#page-and-memory-layout)
  - [Symbol Resolution and Definitions](#symbol-resolution-and-definitions)
  - [Runtime Linking and Loading](#runtime-linking-and-loading)
  - [Security and Hardening](#security-and-hardening)
  - [Linker Behavior and Miscellaneous](#linker-behavior-and-miscellaneous)
- [Linker Script Features (Roadmap)](#linker-script-features-roadmap)
- [Linker Plugins (Roadmap)](#linker-plugins-roadmap)
- [Build Dashboard (Roadmap)](#build-dashboard-roadmap)
- [DeepMap (Roadmap)](#deepmap-roadmap)

---

## Areas for Development

### ELF and Command Line Support
- Expand command‑line option coverage for GNU ld and lld compatibility
- Improve and extend ELF feature support across embedded targets

### Linker Script Features and Compatibility
- Incrementally add GNU linker script keywords and semantics
- Improve compatibility to ease migration from existing toolchains

### Core Linker Capabilities
- LTO integration and performance improvements
- Backward compatibility with existing embedded workflows
- General linker optimizations focused on correctness and scalability

### Target Enablement
- Enable additional architectures and embedded targets
- Support target‑specific features while keeping the core generic

---

## Competitive Readiness and Usability

### Performance and Scalability
- Improve link‑time performance
- Reduce peak memory usage during linking
- Transparent comparisons with other linkers (code size, time, memory)

### User Experience and Diagnostics
- Clear, actionable diagnostics
- Improved reproducibility and supportability
- Richer and structured map file output

### DeepMap Integration
- Open‑source DeepMap for binary layout analysis
- GUI and command‑line interfaces

### Linker Plugin Ecosystem
- Stable and extensible plugin APIs
- Improved plugin diagnostics and observability
- Encourage open‑source plugin development

---

## Building Community Confidence

### Testing and Quality
- Open‑source CI workflows
- Public build and test dashboards

### Documentation
- Clear release notes
- Practical examples for embedded use cases

### Community Engagement
- Easy distribution and discoverability
- Participation in LLVM and embedded communities
- Conference talks, tutorials, and technical deep dives

---

### Legend

- **Status**: `TBD` (unknown / not yet validated), `Planned`, `In Progress`, `Done`
- **GNU ld / lld**: `TBD` (unknown), `Yes`, `No`, `Partial`
- **Reference**: link to upstream documentation (GNU ld manual / lld docs) or ELD docs

> **Note:** The tables below use `TBD` as the default to avoid overstating compatibility until verified.

## Command Line Support (Roadmap)

### Page and Memory Layout

| Option | Description | Status | GNU ld | lld | Reference |
|-------|-------------|--------|--------|-----|-----------|
| `-z common-page-size=SIZE` | Set common page size | TBD | TBD | TBD | |
| `-z max-page-size=SIZE` | Set maximum page size | TBD | TBD | TBD | |
| `-z stack-size=SIZE` | Set stack segment size | TBD | TBD | TBD | |
| `-z separate-code` | Create separate code segment | TBD | TBD | TBD | |
| `-z noseparate-code` | Disable separate code segment (default) | TBD | TBD | TBD | |
| `--rosegment` | With `-z separate-code`, create a single read-only segment | TBD | TBD | TBD | |
| `--no-rosegment` | With `-z separate-code`, create two read-only segments (default) | TBD | TBD | TBD | |

### Symbol Resolution and Definitions

| Option | Description | Status | GNU ld | lld | Reference |
|-------|-------------|--------|--------|-----|-----------|
| `-z defs` | Report unresolved symbols in object files | TBD | TBD | TBD | |
| `-z undefs` | Ignore unresolved symbols in object files | TBD | TBD | TBD | |
| `-z muldefs` | Allow multiple definitions | TBD | TBD | TBD | |
| `-z unique-symbol` | Avoid duplicated local symbol names | TBD | TBD | TBD | |
| `-z nounique-symbol` | Keep duplicated local symbol names (default) | TBD | TBD | TBD | |
| `-z common` | Generate common symbols with `STT_COMMON` type | TBD | TBD | TBD | |
| `-z nocommon` | Generate common symbols with `STT_OBJECT` type | TBD | TBD | TBD | |

### Runtime Linking and Loading

| Option | Description | Status | GNU ld | lld | Reference |
|-------|-------------|--------|--------|-----|-----------|
| `-z global` | Make symbols in DSO available for subsequently loaded objects | TBD | TBD | TBD | |
| `-z initfirst` | Mark DSO to be initialized first at runtime | TBD | TBD | TBD | |
| `-z interpose` | Mark object to interpose all DSOs but executable | TBD | TBD | TBD | |
| `-z unique` | Mark DSO to be loaded at most once by default | TBD | TBD | TBD | |
| `-z nounique` | Don't mark DSO as loadable at most once | TBD | TBD | TBD | |
| `-z lazy` | Mark object lazy runtime binding (default) | TBD | TBD | TBD | |
| `-z now` | Mark object non-lazy runtime binding | TBD | TBD | TBD | |
| `-z loadfltr` | Mark object requiring immediate process | TBD | TBD | TBD | |
| `-z origin` | Mark object requiring immediate `$ORIGIN` | TBD | TBD | TBD | |

### Security and Hardening

| Option | Description | Status | GNU ld | lld | Reference |
|-------|-------------|--------|--------|-----|-----------|
| `-z execstack` | Mark executable as requiring executable stack | TBD | TBD | TBD | |
| `-z noexecstack` | Mark executable as not requiring executable stack | TBD | TBD | TBD | |
| `-z relro` | Create RELRO program header | TBD | TBD | TBD | |
| `-z norelro` | Don't create RELRO program header (default) | TBD | TBD | TBD | |
| `-z text` | Treat `DT_TEXTREL` in output as error | TBD | TBD | TBD | |
| `-z notext` | Don't treat `DT_TEXTREL` in output as error (default) | TBD | TBD | TBD | |
| `-z textoff` | Don't treat `DT_TEXTREL` in output as error (default) | TBD | TBD | TBD | |
| `-z memory-seal` | Mark object to be memory sealed | TBD | TBD | TBD | |
| `-z nomemory-seal` | Don't mark object to be memory sealed (default) | TBD | TBD | TBD | |

### Linker Behavior and Miscellaneous

| Option | Description | Status | GNU ld | lld | Reference |
|-------|-------------|--------|--------|-----|-----------|
| `-z globalaudit` | Mark executable requiring global auditing | TBD | TBD | TBD | |
| `-z start-stop-gc` | Enable garbage collection on `__start/__stop` | TBD | TBD | TBD | |
| `-z nostart-stop-gc` | Don't garbage collect `__start/__stop` (default) | TBD | TBD | TBD | |
| `-z start-stop-visibility=V` | Set visibility of built-in `__start/__stop` symbols | TBD | TBD | TBD | |
| `-z sectionheader` | Generate section header (default) | TBD | TBD | TBD | |
| `-z nosectionheader` | Do not generate section header | TBD | TBD | TBD | |
| `-z combreloc` | Merge dynamic relocs into one section and sort | TBD | TBD | TBD | |
| `-z nocombreloc` | Don't merge dynamic relocs into one section | TBD | TBD | TBD | |
| `-z nocopyreloc` | Don't create copy relocs | TBD | TBD | TBD | |
| `-z nodefaultlib` | Mark object not to use default search paths | TBD | TBD | TBD | |
| `-z nodelete` | Mark DSO non-deletable at runtime | TBD | TBD | TBD | |
| `-z nodlopen` | Mark DSO not available to dlopen | TBD | TBD | TBD | |
| `-z nodump` | Mark DSO not available to dldump | TBD | TBD | TBD | |

---

## Linker Script Features (Roadmap)

| Feature | Priority | Status | GNU ld | lld | Reference |
|--------|----------|--------|--------|-----|-----------|
| `--enable-non-contiguous-regions` | Nice to have | TBD | TBD | TBD | |
| `CLASS` feature | Low | TBD | TBD | TBD | |
| `OVERLAY` | Nice to have | TBD | TBD | TBD | |
| `OVERWRITE_SECTIONS` | Low | TBD | TBD | TBD | |
| `INPUT_SECTION_FLAGS` | Nice to have | TBD | TBD | TBD | |
| `REVERSE` sort policy | Low | TBD | TBD | TBD | |
| Version script complete support | Nice to have | TBD | TBD | TBD | |
| `ASCIZ/ASCIIZ` output section data | Low | TBD | TBD | TBD | |
| `STARTUP` | Low | TBD | TBD | TBD | |
| `NEXT` | Low | TBD | TBD | TBD | |
| `OUTPUT_SECTION` `READONLY` | Low | TBD | TBD | TBD | |
| Output section `TYPE=<value>` | Low | TBD | TBD | TBD | |
| `LD_FEATURE` | Nice to have | TBD | TBD | TBD | |
| `INSERT AFTER/BEFORE` | Nice to have | TBD | TBD | TBD | |
| `SIZEOF_HEADERS` | Nice to have | TBD | TBD | TBD | |

---

## Linker Plugins (Roadmap)

| Task | Priority | Status | GNU ld | lld | Reference |
|------|----------|--------|--------|-----|-----------|
| Improve plugin framework APIs (usability) | Nice to have | TBD | N/A | N/A | |
| Diagnose plugin behavior and issues | Must | TBD | N/A | N/A | |
| Tutorials and demos for plugins | Must | TBD | N/A | N/A | |
| Run plugins in parallel | Nice to have | TBD | N/A | N/A | |
| Inter-plugin communication | Nice to have | TBD | N/A | N/A | |
| Map-file customization via plugins | Nice to have | TBD | N/A | N/A | |
| Plugin-customized linker script commands | Nice to have | TBD | N/A | N/A | |

---

## Build Dashboard (Roadmap)

| Capability | Priority | Status | GNU ld | lld | Reference |
|------------|----------|--------|--------|-----|-----------|
| Open-source dashboard for build workflows using ELD | Must | TBD | N/A | N/A | |
| Filters for pass/fail statistics | Must | TBD | N/A | N/A | |
| Server-less dashboard accessible to the open-source community | Must | TBD | N/A | N/A | |
| Summarized build stats surfaced on GitHub repo home page | Must | TBD | N/A | N/A | |
| Interactive drill-down into failures/issues | Must | TBD | N/A | N/A | |
| Scripts/docs to record build stats in any workflow | Nice to have | TBD | N/A | N/A | |

---

## DeepMap (Roadmap)

| Capability | Priority | Status | GNU ld | lld | Reference |
|------------|----------|--------|--------|-----|-----------|
| Improve plugin APIs for detailed binary map recording | Must | TBD | N/A | N/A | |
| DeepMap GUI app for build/image analysis | Must | TBD | N/A | N/A | |
| Record detailed binary map (bitsery serialization) | Must | TBD | N/A | N/A | |
| Read ELF files in DeepMap | Must | TBD | N/A | N/A | |
| Read JSON files in DeepMap | Nice to have | TBD | N/A | N/A | |
| Documentation for DeepMap | Must | TBD | N/A | N/A | |
| Update DeepMap plugin to integrate with bitsery | Must | TBD | N/A | N/A | |
| Custom Python APIs for binary map | Must | TBD | N/A | N/A | |
| Python command-line tools for binary map | Must | TBD | N/A | N/A | |
| Build/image diff tool | Nice to have | TBD | N/A | N/A | |
| GUI support for common use cases (Zephyr/Modem) | Must | TBD | N/A | N/A | |
| DeepMap VSCode extension | Must | TBD | N/A | N/A | |
| Open-source DeepMap app | Must | TBD | N/A | N/A | |
