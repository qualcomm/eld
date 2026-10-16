# HelloWorldPlugin

HelloWorldPlugin is the minimal ELD linker-plugin example. It demonstrates
plugin registration, initialization, the section and rule visitation hooks,
the output-writing hook, and cleanup. It does not modify the link or require
any plugin options.

## Configuration

Load it through an ELD plugin configuration file:

```yaml
GlobalPlugins:
  - Type: LinkerPlugin
    Name: HelloWorldPlugin
    Library: HelloWorldPlugin
```

With plugin tracing enabled, ELD reports each lifecycle hook, including:

- `Init`
- `VisitSections`
- `ActBeforeRuleMatching`
- `ActBeforeSectionMerging`
- `ActBeforePerformingLayout`
- `ActBeforeWritingOutput`
- `Destroy`

The plugin is intended as a template for new `LinkerPlugin` implementations.

## Build as part of ELD

Configure ELD/LLVM normally, then build the plugin target:

```sh
cmake -S /path/to/llvm-project -B build-eld \
  -DLLVM_ENABLE_PROJECTS=clang
cmake --build build-eld --target HelloWorldPlugin
```

If an ELD build directory already exists, only the final command is needed.
The plugin-local test is registered with the normal ELD test configuration and
can be run through the configured `llvm-lit` driver.

## Build standalone against an installed ELD package

The standalone build uses a host Clang C++ compiler for the plugin shared library.
The target compiler and linker are used by the functional and lit tests:

When `ELD_TARGET_CXX` and `ELD_TARGET_LD` are omitted, CMake derives the
linker path and searches for the target compiler under
`<ELD_PACKAGE_ROOT>/Tools/bin`. Set them explicitly only when the target
compiler or linker is installed somewhere other than the ELD package.

```sh
cmake -S /path/to/eld/Plugins \
  -B build-helloworld \
  -DELD_PACKAGE_ROOT=/path/to/target_tools \
  -DELD_PLUGIN_CXX=/path/to/host-clang++ \
  -DELD_PLUGIN_USE_LIBCXX=ON \
  -DELD_PLUGINS=HelloWorldPlugin
cmake --build build-helloworld
ctest --test-dir build-helloworld --output-on-failure
```

For another supported architecture, set `ELD_TARGET_CXX` and
`ELD_TARGET_LD` only when the target tools are not colocated with the ELD
package. Optionally set `ELD_TARGET_TRIPLE` and the target flag variables.
To build without target tools or tests, add
`-DBUILD_TESTING=OFF`.

### Run the tests with an external `llvm-lit`

The standalone build registers the plugin-local lit suite with CTest. Use an
`llvm-lit` from the external LLVM package, and provide the target triple so
the lit configuration can select the corresponding ELD test target:

```sh
cmake -S /path/to/eld/Plugins \
  -B build-helloworld \
  -DELD_PACKAGE_ROOT=/path/to/target_tools \
  -DELD_PLUGIN_CXX=/path/to/host-clang++ \
  -DELD_PLUGIN_USE_LIBCXX=ON \
  -DELD_PLUGINS=HelloWorldPlugin \
  -DELD_TARGET_TRIPLE=riscv64-unknown-elf \
  -DELD_ENABLE_LIT_TESTS=ON \
  -DELD_LLVM_LIT=/path/to/llvm-package/bin/llvm-lit
cmake --build build-helloworld
ctest --test-dir build-helloworld \
  -R ELDStandaloneLit.HelloWorldPlugin --output-on-failure
```

The standalone lit configuration is self-contained; it does not require the
full ELD source tree or the ELD test harness. The external LLVM tools
directory must provide `FileCheck` and `not`.

`ELD_TEST_TARGET=RISCV64` may be supplied instead of
`ELD_TARGET_TRIPLE` when the automatically derived architecture is not the
one wanted. `ELD_LLVM_LIT` may be omitted when the external `llvm-lit` is on
`PATH`. The tools and libraries used by the lit suite are inferred from the
`llvm-lit` location; set `ELD_LLVM_TOOLS_DIR` or `ELD_LLVM_LIBS_DIR` when
those directories are elsewhere.

The resulting shared library is under
`build-helloworld/HelloWorldPlugin/libHelloWorldPlugin.so`. When loading it
manually with `ld.eld`, make the plugin directory and the package's
`Tools/lib` directory available through `LD_LIBRARY_PATH`.

The package-only smoke test is kept with this plugin at
`test/standalone_test.py`. It is registered with CTest when
`HelloWorldPlugin` is selected.
