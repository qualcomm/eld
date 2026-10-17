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
the compiler or inject runtime flags. Use a fresh build directory when
changing compilers.

FindELD.cmake accepts either a conventional installation with include, lib,
and bin directories or a package containing those directories under Tools.
ELD_ROOT or CMAKE_PREFIX_PATH can also supply the installation prefix.
Only the public plugin headers and host LinkerWrapper library are required;
building plugins does not require LLVM sources, LLVM CMake files, Python,
lit, or a target compiler.

All discovered plugins build by default. Select a subset with
`-DELD_PLUGINS=HelloWorldPlugin`; quote semicolon-separated lists when
selecting multiple plugins.

## Host platforms

Library discovery uses the platform's library suffixes and the installed
API version, rather than a fixed Linux soname. On Windows the installation
must provide both the LinkerWrapper DLL and its import library. ELD_LIBRARY
and ELD_RUNTIME_LIBRARY can override their locations. On Unix, the plugin
records the ELD library directory in its RPATH.

The standalone workflow is validated on Linux. Windows and macOS execution
still require native validation with compatible ELD packages. Compiler,
standard library, architecture, and ABI must match the host ELD installation;
the Linux libc++ command above is not a Windows toolchain configuration.
