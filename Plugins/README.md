# ELD plugins

The plugin directory can be built either as part of ELD or directly against
an installed ELD package.

Plugin discovery is automatic. Every immediate child directory of `Plugins`
that contains a `CMakeLists.txt` is treated as a plugin. The `cmake`,
and `Support` directories are reserved infrastructure and are excluded.
Adding a new plugin therefore requires no edit to this top-level file; give
the plugin directory and CMake target the same name.

## Standalone build

With no `ELD_PLUGINS` selection, all plugins are built:

```sh
cmake -S /path/to/eld/Plugins -B build-plugins \
  -DELD_PACKAGE_ROOT=/path/to/target_tools \
  -DELD_PLUGIN_CXX=/path/to/host-clang++ \
  -DELD_PLUGIN_USE_LIBCXX=ON
cmake --build build-plugins
ctest --test-dir build-plugins --output-on-failure
```

Build a single plugin or a subset with `ELD_PLUGINS`:

```sh
cmake -S /path/to/eld/Plugins -B build-helloworld \
  -DELD_PACKAGE_ROOT=/path/to/target_tools \
  -DELD_PLUGIN_CXX=/path/to/host-clang++ \
  -DELD_PLUGIN_USE_LIBCXX=ON \
  -DELD_PLUGINS=HelloWorldPlugin
cmake --build build-helloworld
ctest --test-dir build-helloworld --output-on-failure
```

`ELD_PACKAGE_ROOT` is the directory containing the installed `Tools`
directory. The standalone project automatically derives `ELD_TARGET_LD` and
searches for `ELD_TARGET_CXX` under `<ELD_PACKAGE_ROOT>/Tools/bin`. Set those
variables explicitly when the target compiler or linker is installed
elsewhere. Set `ELD_TARGET_TRIPLE`, `ELD_TARGET_CXX_FLAGS`, and
`ELD_TARGET_LD_FLAGS` for architecture-specific tool settings.

For lit tests, `ELD_TARGET_CC` is derived from `ELD_TARGET_CXX` when the
compiler names follow the usual `clang`/`clang++` convention. Set it explicitly
if the target C compiler has a different name or location.

`ELD_PLUGIN_CXX` builds the host plugin shared library and should normally be
a Clang C++ compiler. `ELD_PLUGIN_USE_LIBCXX=ON` adds `-stdlib=libc++` before
CMake performs its host compiler checks, and therefore requires a compiler
that supports that option. Set it to `OFF` when the host C++ runtime should
be used instead. Use a fresh build directory when changing the host compiler
or this runtime selection.

`ELD_PLUGINS` is a CMake semicolon-separated list. Quote it when selecting
more than one plugin from a shell.

When plugin lit tests are enabled, the lit architecture is derived from
`ELD_TARGET_TRIPLE`. For example, `riscv64-unknown-elf` selects
`RISCV64_default`, while `hexagon-unknown-none-elf` selects
`Hexagon_default`. Set `ELD_TEST_TARGET` to an architecture such as `RISCV64`,
`Hexagon`, `AArch64`, `ARM`, or `X86` to override the derived value.

Standalone functional tests are registered with CTest and use the target
compiler, `ld.eld`, and Python's standard library. They do not require
`llvm-lit` or `FileCheck`. Use `-DBUILD_TESTING=OFF` to build libraries only.

To also register the plugin lit suites, enable them and provide an external
LLVM `llvm-lit` executable:

```sh
cmake -S /path/to/eld/Plugins -B build-plugins \
  -DELD_PACKAGE_ROOT=/path/to/target_tools \
  -DELD_PLUGIN_CXX=/path/to/host-clang++ \
  -DELD_PLUGIN_USE_LIBCXX=ON \
  -DELD_TARGET_TRIPLE=riscv64-unknown-elf \
  -DELD_ENABLE_LIT_TESTS=ON \
  -DELD_LLVM_LIT=/path/to/llvm-package/bin/llvm-lit \
  -DELD_PLUGINS=HelloWorldPlugin
cmake --build build-plugins
ctest --test-dir build-plugins -R ELDStandaloneLit --output-on-failure
```

`ELD_LLVM_LIT` may be omitted when `llvm-lit` is on `PATH`. The standalone
plugin lit configuration is self-contained and does not load the ELD source
tree's `test/lit.cfg`. It derives the LLVM tool and library directories from
the `llvm-lit` installation; set `ELD_LLVM_TOOLS_DIR` or `ELD_LLVM_LIBS_DIR`
when those locations are separate. The external tools directory must provide
`FileCheck` and `not`; `llvm-readelf` is also needed by tests that inspect ELF
symbols.

Each plugin's README documents its standalone behavior and its plugin-local
standalone smoke test.
