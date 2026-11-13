TODO: add appropriate credit to the existing reloc-test utility and author

Recommended usage:

    ./framework.py --arch=<arch> --mode=<mode> --output-dir=<output-directory> >report.txt

where
* `<arch>` - architecture (`x86_64` or `arm`).
* `<output-directory>` - directory where work files are created.
* `<mode>` - analysis mode: `static`, `run` or `both` (default: `both`).

The configurations use `clang`, `ld.eld` and `llvm-readelf` from `PATH`. ARM
runtime tests also require `qemu-arm`.
