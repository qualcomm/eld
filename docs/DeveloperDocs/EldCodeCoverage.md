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

### Step 5 — Extract Coverage Percentages

```bash
COV_SUMMARY=$(llvm-cov report \
  --instr-profile=obj/eld.profdata \
  --object obj/lib/libLW.so \
  --include-filename-regex='/eld/')

TOTAL_LINE=$(echo "$COV_SUMMARY" | grep '^TOTAL')
echo "$TOTAL_LINE" #To get the final line which has the results

# llvm-cov report writes only to stdout — there is no -o flag to save output.
# So we capture the full report into $COV_SUMMARY via command substitution,
# grep the TOTAL line, then parse each percentage by column.
REGION_COV=$(echo "$TOTAL_LINE" | awk '{print $4}')
FUNC_COV=$(echo   "$TOTAL_LINE" | awk '{print $7}')
LINE_COV=$(echo   "$TOTAL_LINE" | awk '{print $10}')
BRANCH_COV=$(echo "$TOTAL_LINE" | awk '{print $13}')

echo "Line:     $LINE_COV"
echo "Function: $FUNC_COV"
echo "Region:   $REGION_COV"
echo "Branch:   $BRANCH_COV"
```
