# ELD Code Coverage Guide

## LLVM Native Coverage (llvm-cov source-based)

This pipeline uses Clang's source-based coverage instrumentation, which provides finer-grained (expression-level) coverage data.

> **Toolchain requirement**: This pipeline requires the **LLVM/Clang toolchain** (`clang`/`clang++`). It is not compatible with GCC or the GNU toolchain.

The pipeline relies on two artifact types:

- **`.profraw`** — Generated at **runtime** by each executed binary. Contains raw profile counters per function region.
- **`.profdata`** — Merged from one or more `.profraw` files using `llvm-profdata merge`. This is the indexed form that `llvm-cov` consumes.

## Prerequisites

Ensure `llvm-cov` and `llvm-profdata` are available:

```bash
llvm-cov --version
llvm-profdata --version
```
Export the paths of clang and clang++

```bash
export CC=clang
export CXX=clang++
```

---

## Code Coverage Build

Note:
- We use the CMake option `ELD_COVERAGE=ON` which enables the coverage flags (`-fprofile-instr-generate -fcoverage-mapping`).
- `CMAKE_BUILD_TYPE=Release` can also be used for source-based coverage — Clang's instrumentation is inserted before the optimizer runs and is immutable, so optimizations do not affect report accuracy. See [Clang source-based coverage docs](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html).

```bash
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLLVM_ENABLE_ASSERTIONS:BOOL=ON \
  -DLLVM_ENABLE_PROJECTS='llvm;clang' \
  -DLLVM_EXTERNAL_PROJECTS=eld \
  -DLLVM_EXTERNAL_ELD_SOURCE_DIR=${PWD}/llvm-project/eld \
  -DLLVM_TARGETS_TO_BUILD='ARM;AArch64;RISCV;Hexagon;X86' \
  -DCMAKE_CXX_FLAGS='-stdlib=libc++' \
  -DLLVM_USE_LINKER=lld \
  -DELD_ENABLE_SYMBOL_VERSIONING=ON \
  -DELD_ENABLE_RUN_TESTS=ON \
  -DELD_COVERAGE=ON \
  -B ./obj \
  -S ./llvm-project/llvm
```

Build:

```bash
cmake --build obj
```

---

## Generating Code Coverage Report

### Step 1 — Run Tests with Profile Output

Set `LLVM_PROFILE_FILE` so every test process writes its raw profile data to a unique file. The `%p` token expands to the process ID, preventing parallel test processes from overwriting each other:

```bash
LLVM_PROFILE_FILE="$PWD/obj/eld-%p.profraw" cmake --build obj -- check-eld
```

### Step 2 — Merge Profile Data

Merge all `.profraw` files into a single indexed `.profdata` file. The `-sparse` flag omits zero counters to keep the output compact:

```bash
llvm-profdata merge -sparse obj/eld-*.profraw -o obj/eld.profdata
```

### Step 3 — Generate HTML Report

Run `llvm-cov show` to produce an annotated HTML report. We pass `libLW.so` as the instrumented object — all ELD lib, include, and tools source code is bundled into `libLW.so` at link time.

The `--include-filename-regex` filter includes only source files whose path contains `/eld/`, covering both `llvm-project/eld/` and cmake-generated headers in `obj/tools/eld/`.

> **Note**: `--include-filename-regex` was introduced in LLVM on 2026-01-27
> ([#175779](https://github.com/llvm/llvm-project/pull/175779)) and will be
> available in LLVM 23 and later. For older toolchains use
> `--ignore-filename-regex` instead to exclude non-ELD paths.
> For LLVM < 23, replace `--include-filename-regex='/eld/'` with `--ignore-filename-regex='llvm-project/(llvm|clang)/|third-party/|/obj/tools/[^e]|/obj/tools/e[^l]|/obj/tools/el[^d]|/obj/(include|lib|bin)/|llvm-project/eld/test/|llvm-project/eld/Plugins/HelloWorldPlugin/'`.

```bash
llvm-cov show \
  --format=html \
  --instr-profile=obj/eld.profdata \
  --object obj/lib/libLW.so \
  --show-branches=count \
  --show-expansions \
  --show-instantiations \
  --include-filename-regex='/eld/' \
  -o coverage-report
```

### Step 4 — Quick Coverage Summary

Print line, function, region, and branch coverage percentages:

```bash
llvm-cov report \
  --instr-profile=obj/eld.profdata \
  --object obj/lib/libLW.so \
  --include-filename-regex='/eld/'
```
