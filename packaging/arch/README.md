# Arch Linux Package for ELD

This directory contains the files needed to submit ELD to the
[Arch User Repository (AUR)](https://aur.archlinux.org/).

## Files

- `PKGBUILD` - The package build script
- `.SRCINFO` - Machine-readable metadata (regenerated before each push)

## Prerequisites

1. An Arch Linux system (or VM/container) with `base-devel` installed
2. An [AUR account](https://aur.archlinux.org/register) with SSH key configured
3. Tools: `makepkg`, `namcap` (from `namcap` package)

## Local Testing

```bash
# Copy PKGBUILD to a clean directory
mkdir /tmp/eld-pkg && cp PKGBUILD /tmp/eld-pkg/ && cd /tmp/eld-pkg

# Build the package locally (downloads sources, builds, creates .pkg.tar.zst)
makepkg -s

# Lint the PKGBUILD
namcap PKGBUILD

# Lint the built package
namcap eld-*.pkg.tar.zst

# Install locally to test
makepkg -si
```

For maximum correctness, build in a clean chroot:

```bash
# Requires 'devtools' package
extra-x86_64-build
```

## Submitting to the AUR

```bash
# 1. Clone the AUR git repo (first time only)
git clone ssh://aur@aur.archlinux.org/eld.git
cd eld

# 2. Copy the PKGBUILD
cp /path/to/PKGBUILD .

# 3. Generate .SRCINFO (REQUIRED on every push)
makepkg --printsrcinfo > .SRCINFO

# 4. Commit and push
git add PKGBUILD .SRCINFO
git commit -m "Initial upload: eld 22.1.0rc3-1"
git push origin master
```

## Updating the Package

When a new eld version is released:

1. Update `pkgver` in `PKGBUILD`
2. Update `_llvmver` if the LLVM compatibility version changed
3. Reset `pkgrel` to `1`
4. Update checksums: `updpkgsums` (from `pacman-contrib`)
5. Regenerate `.SRCINFO`: `makepkg --printsrcinfo > .SRCINFO`
6. Test locally, then commit and push to AUR

## LLVM Version Compatibility

ELD must be built within the LLVM source tree (placed in `llvm/tools/eld`).
ELD's version number is derived from LLVM's `PACKAGE_VERSION`, so the
versions are intentionally synchronized. The eld `release/N.x` branch
tracks the `release/N.x` branch of `llvm/llvm-project`.

When LLVM has a stable release tarball available, the PKGBUILD can use a
tarball source instead of the git clone. When only an RC is available (as
with 22.x), the PKGBUILD uses a git source for the `release/N.x` branch.

## Package Layout

The built package installs:

```
/usr/bin/ld.eld                        # Main linker binary
/usr/bin/arm-link -> ld.eld            # Target symlinks
/usr/bin/aarch64-link -> ld.eld
/usr/bin/hexagon-link -> ld.eld
/usr/bin/riscv-link -> ld.eld
/usr/bin/x86_64-link -> ld.eld
/usr/bin/LSParserVerifier              # Linker script parser verifier
/usr/bin/YAMLMapParser.py              # YAML map file parser
/usr/lib/libLW.so.4                    # Plugin API shared library
/usr/lib/libLW.so -> libLW.so.4
/usr/include/ELD/PluginAPI/*.h         # Plugin development headers
/usr/share/eld/templates/              # Linker script templates
/usr/share/licenses/eld/LICENSE        # BSD 3-Clause license
```
