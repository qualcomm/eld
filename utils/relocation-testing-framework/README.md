TODO: add appropiate credit to the existing reloc-test utility and author

Recommended usage:

    ./framework.py --arch=<arch> --ld=<linker> --mode=<mode> --output-dir=<output-directory> >report.txt

where
* `<arch>` - architecture.
* `<linker>` - linker invocation path, may include command line options. Example: `aarch64-linux-gnu-ld`.
* `<output-directory>` - directory where work files are created.
* `<mode>` - analyzis mode. options are static, run or both.

The default path must include some tools:
* `ld.lld`, used as a default linker