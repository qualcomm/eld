# ELD: Embedded LD

ELD is an ELF linker designed to meet the needs of embedded software projects.
It aims to be a drop-in replacement for the GNU linker, with a smaller memory
footprint, faster link times and a customizable link behavior.

ELD supports targets Hexagon, ARM, AArch64 and RISCV
and is designed for easy addition of more backends.

## Supported features
- Static linking
- Dynamic linking
- Partial linking
- LTO
- Same command-line options as GNU LD.
- Linker scripts. Linker script syntax is same as of GNU LD.
- Highly detailed linker map-files.

  Highly detailed map-files are essential to debug complex image layouts.
- Linker plugins

  Linker plugins allow a user to programmatically customize link
  behavior for advanced use-cases and complex image layouts.
- Reproduce functionality

  The reproduce functionality creates a tarball that can be used to reproduce the
  link without any other dependency.

## Building ELD and running tests

ELD supports building and running tests on Linux and Windows utilizing LLVM.

ELD depends on LLVM. You can build ELD either:
- Integrated into an `llvm-project` build (recommended for running tests), or
- Against an external, prebuilt LLVM installation (faster iteration if you already have an LLVM release).

You will need a recent C++ compiler for building LLVM and ELD.

```
git clone https://github.com/llvm/llvm-project.git
cd llvm-project/
git clone https://github.com/qualcomm/eld.git
cd ../
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX=../inst \
  -DLLVM_ENABLE_ASSERTIONS:BOOL=ON \
  -DLLVM_ENABLE_PROJECTS='llvm;clang' \
  -DLLVM_EXTERNAL_PROJECTS=eld \
  -DLLVM_EXTERNAL_ELD_SOURCE_DIR=${PWD}/llvm-project/eld \
  -DLLVM_TARGETS_TO_BUILD='ARM;AArch64;RISCV;Hexagon;X86' \
  -DCMAKE_CXX_FLAGS='-stdlib=libc++' \
  -B ./obj \
  -S ./llvm-project/llvm
cmake --build obj # Build the linker
cmake --build obj -- check-eld # Build test artifacts and run the tests
```

## Building ELD with an external LLVM installation

If you already have an installed LLVM/Clang "release" directory (with `bin/clang`, `bin/clang++`, `bin/llvm-tblgen`, and `lib/cmake/llvm`), you can configure ELD directly against it:

```bash
./configure_external_llvm.sh <LLVM_RELEASE_DIR> <ELD_BUILD_DIR>
cd <ELD_BUILD_DIR>
ninja
```

Notes:
- Tests are disabled by default for external builds;
- To build just the linker: `ninja ld.eld`

Some (optional) helpful CMake options:

- `-DLLVM_USE_SPLIT_DWARF=On`

  This option helps save on disk space with debug builds

- `-DLLVM_PARALLEL_LINK_JOBS`

  Linking LLVM toolchain can be very memory-intensive. To avoid OOM or swap performance repurcussion, it is recommended to restrict parallel link jobs, especially in systems with less than 32 GB of memory.

- `-DLLVM_USE_LINKER=lld`

  Using LLD instead of GNU LD can signficantly help in reducing link time and memory.

### Running a Single Test Case
To execute an individual ELD test without running the entire suite, follow the steps below from the build directory ('obj/' here).

#### 1. Locate the Test Configuration and Test File

- **Test configurations** are stored under:
`obj/tools/eld/test/`

- **Test files** reside in the source tree, for example:
`llvm-project/eld/test/<Subdirectory>/<TestName>.test`

#### 2. Invoke the Test Harness

Use the following command to run a specific test case:

```bash
<path_to_lit_config> <path_to_test_file>
```

- `<path_to_lit_config>` : Path to the LIT configuration script
- `<path_to_test_file>` : Path to the '.test' file you wish to execute

#### Example
```bash
obj/tools/eld/test/llvm-lit-arm-default  llvm-project/eld/test/Common/standalone/AliasSymbolExportDynamic/AliasSymbolExportDynamic.test
```

This command runs only the AliasSymbolExportDynamic test using the ARM default configuration. Results and diagnostics will be printed to the console.

#### Enabling Verbose Output

- -a: Always show detailed output.
- -v: Show detailed output only when a test fails ("on failure" mode).

### Testing Vim Integration

The Vim syntax-highlighting files in `etc/vim/` have a
[lit](https://llvm.org/docs/CommandGuide/lit.html)-based test suite that
verifies syntax-group assignments for every token class in `eld.vim`.

**Prerequisites**

- Vim 9.1 or later in `PATH` (tests drive Vim in batch mode; Neovim is not supported).
- Python 3 in `PATH`.
- `lit` installed (`pip install lit`).

**Running the tests**

Each test spawns one Vim process; run the suite serially to avoid resource contention:

```bash
cd etc/vim/test
lit -j1 .
```

Run a single test file:

```bash
lit -j1 comments.test
```

Run with verbose output to see per-check PASS/FAIL lines:

```bash
lit -j1 -v .
```

Use a specific Vim binary:

```bash
lit -j1 -D vim=/path/to/vim .
```

**CMake build target**

Enable the test suite at configure time with `-DELD_VIM_TESTS=ON`.
This adds a `check-eld-vim` target that runs the tests with a single worker
and a 120-second per-test timeout:

```bash
cmake -DELD_VIM_TESTS=ON <other options> <source-dir>
ninja check-eld-vim
```

### Building documentation

First install the prerequisites for building documentation:

- Doxygen (>=1.8.11)
- graphviz
- Sphinx and other python dependencies as specified in 'docs/userguide/requirements.txt'

On an ubuntu machine, the prerequisites can be installed as:

```
sudo apt install doxygen graphviz
pip3 install -r ${ELDRoot}/docs/userguide/requirements.txt
```

Finally, to build documentation:

- Configure CMake with the option `-DLLVM_ENABLE_SPHINX=On`
- Build documentation by building `eld-docs` target: `cmake --build . --target eld-docs`

## Running DCO Checks Locally

To run DCO checks locally you will need to fiest install nodeJs and npm:

On an ubuntu machine, these can be installed as:

```
sudo apt install nodejs
sudo apt install npm
```
You can then install repolinter using:

```
npm install repolinter
```

To run the DCO checks locally use the following command:

```
repolinter lint </path/to/eld>
```

## Clang-format

ELD uses LLVM clang-format style via the repository `.clang-format` file.

### Bash functions

Use the helper script at:
`etc/bash/eld_clang_format_helpers.sh`

To enable the functions in your shell, add this to your `~/.bashrc`
(or `~/.bash_aliases`) and reload:

```bash
source </path/to/eld>/etc/bash/eld_clang_format_helpers.sh
```

For usage, run:

```bash
eld_clang_format_check --help
```

### Check formatting locally

```bash
clang-format --version
eld_clang_format_check <base-branch>
```

`eld_clang_format_check` prints per-file colored status and returns non-zero on
failure.

### Format changed files locally

```bash
eld_clang_format_fix <base-branch>
```

### CI enforcement

Pull requests run `.github/workflows/clang-format-pr.yml`, which fails if any
changed C/C++ source file is not formatted with `clang-format --style=file`.

## Clang-tidy and cpp-linter

ELD PRs also run clang-tidy-based lint checks via:
`.github/workflows/cpp-linter-pr.yml`

Current behavior:
- Runs on changed C/C++ files in the PR.
- Uses external-LLVM build setup to generate `compile_commands.json`.
- Runs non-blocking for now (does not fail the PR by default).
- Uploads downloadable logs/artifacts (including detailed diagnostics).

### Local helper functions

Use the helper script at:
`etc/bash/eld_cpp_linter_helpers.sh`

To enable the functions in your shell:

```bash
source </path/to/eld>/etc/bash/eld_cpp_linter_helpers.sh
```

For usage/help:

```bash
eld_cpp_linter_check --help
```

Common usage:

```bash
# Check changed files against base branch
eld_cpp_linter_check --base-branch main --build-directory <build-dir>

# Apply clang-tidy fixes on changed files
eld_cpp_linter_fix --base-branch main --build-directory <build-dir>

# Check or fix a single file
eld_cpp_linter_check --build-directory <build-dir> --file path/to/file.cpp
eld_cpp_linter_fix   --build-directory <build-dir> --file path/to/file.cpp
```

### How to skip files from clang-tidy checks

1. Skip in PR workflow input list (temporary CI-level skip):

```bash
git diff --name-only --diff-filter=ACMRT "origin/${BASE_REF}...HEAD" \
  | grep -E '\.(c|cc|cpp|cxx|h|hh|hpp|hxx|inc|def)$' \
  | grep -Ev '^(path/to/file1\.cpp|path/to/legacy/.*)$' \
  > .cpp-linter-files.txt || true
```

2. Run helpers on a specific file only (local scope control):

```bash
eld_cpp_linter_check --build-directory <build-dir> --file path/to/file.cpp
```

3. Add source-level suppressions where justified:
- `// NOLINT(<check-name>)`
- `// NOLINTBEGIN(<check-name>)` / `// NOLINTEND(<check-name>)`

4. Use repo-level `.clang-tidy` policy filters, if maintained, for broad path/check tuning.

### Where to find clang-tidy check names

- Official clang-tidy checks list:
  `https://clang.llvm.org/extra/clang-tidy/checks/list.html`
- Checks supported by your installed clang-tidy version:

```bash
clang-tidy -list-checks -checks='*'
```

## ELD Build Status

Live status of workflows building with ELD

[![Getting latest build status...](https://raw.githubusercontent.com/qualcomm/eld/documentation/dash/dashboard.png)](https://qualcomm.github.io/eld/dash/dash.html)


## License

Licensed under [BSD 3-Clause License](LICENSE)
