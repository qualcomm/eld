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

ELD depends on LLVM. It is required to build LLVM when building ELD.

You will need a recent C++ compiler for building LLVM and ELD.

```
git clone https://github.com/llvm/llvm-project.git
cd llvm-project/
git clone https://github.com/qualcomm/eld.git
cd ../
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX=../inst \
  -DLLVM_ENABLE_ASSERTIONS:BOOL=OFF \
  -DLLVM_ENABLE_PROJECTS='llvm;clang' \
  -DLLVM_EXTERNAL_PROJECTS=eld \
  -DLLVM_EXTERNAL_ELD_SOURCE_DIR=${PWD}/llvm-project/eld \
  -DLLVM_TARGETS_TO_BUILD='ARM;AArch64;RISCV;Hexagon' \
  -DCMAKE_CXX_FLAGS='-stdlib=libc++' \
  -B ./obj \
  -S ./llvm-project/llvm
cmake --build obj # Build the linker
cmake --build obj -- check-eld # Build test artifacts and run the tests
```

Some (optional) helpful CMake options:

- `-DLLVM_USE_SPLIT_DWARF=On`

  This option helps save on disk space with debug builds

- `-DLLVM_PARALLEL_LINK_JOBS`

  Linking LLVM toolchain can be very memory-intensive. To avoid OOM or swap performance repurcussion, it is recommended to restrict parallel link jobs, especially in systems with less than 32 GB of memory.

- `-DLLVM_USE_LINKER=lld`

  Using LLD instead of GNU LD can signficantly help in reducing link time and memory.

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

## License

Licensed under [BSD 3-Clause License](LICENSE)
