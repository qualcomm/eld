# ELD plugins

Plugins build as part of ELD or against an installed ELD package. Every
immediate child directory containing a CMakeLists.txt is discovered
automatically, except the reserved cmake and Support directories.

## Build as part of ELD

Configure ELD normally, then build the desired target:

```sh
cmake --build /path/to/build-eld --target HelloWorldPlugin
```

## Build against an installed package

Select standalone mode explicitly:

```sh
cmake -S /path/to/eld/Plugins -B build-plugins \
  -DBUILD_STANDALONE_PLUGINS=ON \
  -DELD_PACKAGE_ROOT=/path/to/target_tools \
  -DCMAKE_CXX_COMPILER=/path/to/host-clang++ \
  -DCMAKE_CXX_FLAGS="-stdlib=libc++"
cmake --build build-plugins
```

The plugin runs on the host. Select a host C++ compiler, and use libc++ as
required by the ELD package. Set the compiler and runtime flags with standard
CMake variables or a host toolchain file so they apply during CMake's
compiler checks as well as the plugin build. The harness does not select
the compiler or C++ headers. Use a fresh build directory when changing
compilers or ELD packages.

FindELD.cmake accepts either a conventional installation with include, lib,
and bin directories or a package containing those directories under Tools.
ELD_ROOT or CMAKE_PREFIX_PATH can also supply the installation prefix.
The public plugin headers and host LinkerWrapper library are required; the
Linux build also requires the package's libc++ link files (see below).
This foundation builds plugins only. It does not configure or run CTest or
lit; standalone test support supplies those tools and target compilers in a
subsequent layer. Building plugins does not require LLVM sources, LLVM CMake
files, Python, lit, or a target compiler.

All discovered plugins build by default. Select a subset with
`-DELD_PLUGINS=HelloWorldPlugin`; quote semicolon-separated lists when
selecting multiple plugins. This lets CI select plugins compatible with an
older ELD package. A plugin can call
`eld_standalone_require_api(plugin header symbol_regex)` to reject a package
that lacks a required public API during configuration. `plugin` is the target
name printed in the diagnostic, `header` is relative to the installed
`ELD_INCLUDE_DIR` (for example `LinkerWrapper.h`), and `symbol_regex` is a
CMake regular expression searched in that header. Escape regular-expression
metacharacters when matching a literal spelling. The helper checks API
availability early; compilation remains responsible for validating signatures.
Use `ELD_PLUGINS` to omit an incompatible plugin from that package build.

### Package C++ runtime libraries (Linux)

On Linux, the standalone framework automatically prepends the package's host
library directory to link-time search paths, **before `project()`**, so CMake's
compiler probes and executable, shared-library and module links select the
same runtime. Continue to pass `CMAKE_CXX_FLAGS=-stdlib=libc++` with a compatible
host Clang. This selects libraries, not headers, the compiler, or target
test tools. Configuration checks that libc++ headers and the selected libraries
can compile and link a small C++ program.

Discovery accepts `ELD_PACKAGE_ROOT`, `ELD_ROOT`, or `CMAKE_PREFIX_PATH`, with
`lib`/`lib64` and `Tools/lib`/`Tools/lib64` layouts. Set these before `project()`
(on the command line or in a preset). `ELD_CXX_RUNTIME_DIR` overrides discovery
for a custom **host** runtime directory. Both `libc++.so` and `libc++abi.so`
must be present; missing runtime files cause an error rather than a fallback to
system libraries. Some packages ship the first as a linker script, which is
honored rather than bypassed by linking a versioned shared library directly.

Plugins are loaded into the ELD linker's process, so their C++ runtime must be
compatible with the runtime packaged with that linker. Using the build host's
default `libc++` can silently introduce a different `libc++abi`, unwinder, or ABI
requirement. A successful build, or matching `libc++.so.1` SONAMEs, does not prove
compatibility. Failures can include missing libraries, unresolved symbols, or
runtime failures when C++ objects or exceptions cross the plugin boundary.

For example, a host system's `libc++.so` linker script may add `-lunwind`, recording
a dependency on `libunwind.so.1`, while the ELD package's script selects a
`libc++abi` built for `libgcc_s` without that dependency. A machine providing
`libunwind.so.8` does not necessarily satisfy `libunwind.so.1`; do not substitute
SONAMEs with symlinks as an ABI fix. `-stdlib=libc++` alone does not distinguish
host and package libraries. RPATH or `LD_LIBRARY_PATH` controls runtime lookup,
but cannot remove an unwanted dependency recorded at link time. Build against
the linker's packaged runtime and use the same compatible runtime when loading.

User linker flags are preserved after the managed `-L` prefix. Reconfiguring
does not duplicate it, and changing packages replaces the old prefix. The
selected directory is also included in standalone tests' runtime search paths,
but **no RPATH or RUNPATH is embedded in standalone plugins**, in either build
or install artifacts. The loading environment must supply the runtime search
path, for example through `LD_LIBRARY_PATH` on Linux, with the package library
directory before unrelated host runtime directories. CTest arranges it for tests.
Avoid passing explicit `-Wl,-rpath,...` linker flags: the framework suppresses
CMake-generated RPATHs, but does not rewrite user-supplied linker flags.
This does **not** make different ELD/C++ ABIs compatible or replace the need
to select compatible headers. Cross-package deployment still requires ABI
validation. Non-Linux hosts use their native host toolchain configuration.
Package runtime selection on Linux is built-in behavior, not an opt-in option.

To check actual dependencies on Linux:

```sh
readelf -d build-plugins/HelloWorldPlugin/libHelloWorldPlugin.so
```

`NEEDED` lists the selected SONAMEs. Whether libunwind appears depends on the
package's runtime configuration; its absence is not a universal requirement.
There should be no `RPATH` or `RUNPATH` dynamic tag. When a host readelf is
available, the `ELDStandaloneRuntime.*` CTest entries verify this for each plugin.
`ELDStandalone.RuntimeConfiguration` checks search-path setup and cleanup when
Linux standalone testing is enabled.

## Host platforms

Library discovery uses the platform's normal library suffixes to find `LW`.
The host linker records the SONAME of the selected library. On Windows the
installation must provide both the LinkerWrapper DLL and its import library.
ELD_LIBRARY and ELD_RUNTIME_LIBRARY can override their locations. Standalone
plugins do not record library search paths; set up the runtime environment
before loading.

The standalone workflow is validated on Linux. Windows and macOS execution
still require native validation with compatible ELD packages. Compiler,
standard library, architecture, and ABI must match the host ELD installation;
the Linux libc++ command above is not a Windows toolchain configuration.

## Standalone tests

Building libraries is the default; test dependencies are discovered only with
`BUILD_TESTING=ON`. Copying the Plugins directory is sufficient: standalone
tests do not load the ELD source-tree test harness or require LLVM sources.

The host compiler above builds the plugin loaded into ELD. The separate
`ELD_TARGET_CC`, `ELD_TARGET_CXX`, and `ELD_TARGET_LD` settings build and
link target test inputs. They are not replacements for CMAKE_CXX_COMPILER.
Test setup searches the resolved package's bin directory for clang/clang++
first, with hexagon-clang/hexagon-clang++ fallbacks, and for ld.eld. Explicit
paths override discovery when the tools are elsewhere. If no separate C
driver is found, the C++ driver is also used for C inputs.

Enable package-only smoke tests (Python 3, a target compiler, and ELD):

```sh
cmake -S /path/to/eld/Plugins -B build-plugins \
  -DBUILD_STANDALONE_PLUGINS=ON \
  -DELD_PACKAGE_ROOT=/path/to/target_tools \
  -DCMAKE_CXX_COMPILER=/path/to/host-clang++ \
  -DCMAKE_CXX_FLAGS="-stdlib=libc++" \
  -DELD_PLUGINS=HelloWorldPlugin \
  -DBUILD_TESTING=ON
cmake --build build-plugins
ctest --test-dir build-plugins --output-on-failure
```

For lit suites, add an external lit runner and target selection:

```sh
cmake -S /path/to/eld/Plugins -B build-plugins \
  -DBUILD_STANDALONE_PLUGINS=ON \
  -DELD_PACKAGE_ROOT=/path/to/target_tools \
  -DCMAKE_CXX_COMPILER=/path/to/host-clang++ \
  -DCMAKE_CXX_FLAGS="-stdlib=libc++" \
  -DELD_PLUGINS=HelloWorldPlugin \
  -DBUILD_TESTING=ON \
  -DELD_ENABLE_LIT_TESTS=ON \
  -DELD_TARGET_TRIPLE=riscv64-unknown-elf \
  -DELD_LLVM_LIT=/path/to/llvm-package/bin/llvm-lit
cmake --build build-plugins
ctest --test-dir build-plugins --output-on-failure
```

Use `ctest -R ELDStandaloneLit` to run only lit or
`ctest -R '^ELDStandalone[.]'` to run only smoke tests. For multi-configuration
generators, build with `--config Release` and test with `ctest -C Release`.

ELD_LLVM_LIT may be omitted when llvm-lit (or lit) is on PATH. The LLVM tools
directory defaults to the runner's directory and the library directory to
its sibling lib. Override ELD_LLVM_TOOLS_DIR and ELD_LLVM_LIBS_DIR when needed,
including when lit was installed separately with pip. Lit additionally
requires FileCheck and not. llvm-readelf is optional; tests that use it must
declare `REQUIRES: llvm-readelf`. HelloWorld does not require it.

Plugins/lit.cfg derives its architecture from ELD_TARGET_TRIPLE. Supported
names include ARM, AArch64, Hexagon, RISCV, RISCV64, and X86. ELD_TEST_TARGET
overrides the lit architecture (for example RISCV64 or RISCV64_default);
it does not override an explicitly supplied compiler target triple. Use
ELD_TARGET_TRIPLE to select the same target for smoke and lit tests.
ELD_TARGET_CXX_FLAGS supplies additional flags to both target compiler
drivers; ELD_TARGET_LD_FLAGS supplies target linker flags. Host compiler
flags are independent of these settings.

CTest and lit arrange runtime library search paths: LD_LIBRARY_PATH on
Linux, DYLD_LIBRARY_PATH on macOS, and PATH on Windows. The same paths are
needed when loading a plugin manually. LLVM helper tools run on the host
and must be compatible with the target object formats.

## Adding a plugin

Give the plugin directory and CMake target the same name. Use
`eld_add_standalone_plugin(... SOURCES ... LIBRARIES ...)` when
ELD_STANDALONE is true and the normal ELD/LLVM library helpers otherwise.
The standalone helper links ELD::LW; optional dependencies belong to the
plugin that needs them. `eld_standalone_require_api(plugin header symbol)`
can reject packages missing a required public API.

Keep regression tests under the plugin's test directory. Register them with
`eld_register_plugin_tests(test_dir target)` in integrated builds, and with
`eld_add_standalone_test(NAME ... TARGET ... SCRIPT ...)` and
`eld_add_standalone_lit_test(NAME ... TARGET ... TEST_DIR ...)` when
standalone BUILD_TESTING is enabled. See HelloWorldPlugin for an example.
The smoke helper passes package, compiler, linker, flags, and library-path
arguments to the Python script; the lit helper uses Plugins/lit.cfg.
