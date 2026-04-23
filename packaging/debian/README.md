# Debian Package for ELD

This directory contains the Debian packaging metadata for ELD.

## Files

- `source/format` - Source package format (`3.0 (quilt)`)
- `control` - Source and binary package definitions
- `rules` - Build rules (CMake + Ninja via debhelper)
- `changelog` - Package version history
- `copyright` - DEP-5 formatted license information
- `watch` - Upstream release monitoring for `uscan`
- `eld.install` - File list for the `eld` binary package
- `eld-plugin-dev.install` - File list for the `eld-plugin-dev` headers package
- `eld.docs` - Documentation files shipped with `eld`

## Prerequisites

1. A Debian or Ubuntu system with packaging tools installed:
   ```bash
   sudo apt install build-essential debhelper devscripts lintian
   ```
2. Build dependencies (installed automatically by `sbuild`, or manually):
   ```bash
   sudo apt install cmake ninja-build llvm-19-dev libclang-19-dev \
     llvm-19-tools python3 zlib1g-dev
   ```

## Building

Since `dpkg-buildpackage` requires `debian/` at the source root, create a
symlink before building:

```bash
cd /path/to/eld
ln -s packaging/debian debian
dpkg-buildpackage -us -uc
```

For a clean chroot build (recommended):

```bash
cd /path/to/eld
ln -s packaging/debian debian
sbuild --no-clean-source
```

## Linting

```bash
# Lint the source package
lintian --info *.dsc

# Lint the binary packages
lintian --info *.deb
```

## Binary Packages

The packaging produces two binary packages:

- **`eld`** — The linker binary, libLW shared library, linker script
  templates, and YAMLMapParser utility
- **`eld-plugin-dev`** — Plugin API C++ development headers

## Package Layout

```
/usr/bin/ld.eld                        # Main linker binary
/usr/bin/YAMLMapParser.py              # YAML map file parser
/usr/lib/*/libLW.so*                   # Plugin API shared library
/usr/share/eld/templates/              # Linker script templates
/usr/include/ELD/PluginAPI/*.h         # Plugin development headers (eld-plugin-dev)
/usr/share/doc/eld/About.md            # Project documentation
```

## LLVM Version Compatibility

ELD derives its version from LLVM's `PACKAGE_VERSION`. The `debian/rules`
file passes `-DPACKAGE_VERSION=19.0.0` and points to the LLVM 19 CMake
config at `/usr/lib/llvm-19/lib/cmake/llvm`. When updating to a new LLVM
version, update these values along with the Build-Depends in `control`.
