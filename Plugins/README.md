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
does not duplicate it, and changing packages replaces the old prefix.
**No RPATH or RUNPATH is embedded in standalone plugins**, in either build
or install artifacts. The loading environment must supply the runtime search
path, for example through `LD_LIBRARY_PATH` on Linux, with the package library
directory before unrelated host runtime directories.
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
There should be no `RPATH` or `RUNPATH` dynamic tag.

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
