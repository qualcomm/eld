# Chroot support

`--chroot <directory>` makes ELD resolve absolute input paths beneath
`<directory>`. For example, with `--chroot /sdk`, an input path of
`/usr/lib/libfoo.a` is read from `/sdk/usr/lib/libfoo.a`.

The key motivation for adding this feature is to support lld reproducers,
which make use of the `--chroot` option.

It applies to all the input paths that the linker sees such as input files, 
linker scripts and their includes, search-directory lookups such as 
`-L /usr/lib -lfoo`, plugin configuration, and plugin libraries. 
**Relative paths are used unchanged, and output paths are not remapped.**

This is linker path remapping, not the Linux `chroot(2)` system call. It does
not change the process root or provide isolation.

## Interaction with `--remap-inputs`

ELD applies `--remap-inputs` before `--chroot`. A remapped absolute path is
therefore resolved beneath the chroot as expected. For example:

```bash
--remap-inputs=/build/foo.o=/cache/foo.o --chroot /sdk /build/foo.o
```

Linekr reads `/sdk/cache/foo.o`.

## Interaction with `--sysroot`

ELD expands `=` and `$SYSROOT` path markers using `--sysroot` before applying
`--chroot`. For example:

```text
--sysroot /toolchain --chroot /sdk =/usr/lib/libfoo.a
```

reads `/sdk/toolchain/usr/lib/libfoo.a`. The same ordering applies to search
directories such as `-L=/usr/lib`.

## Diagnostics

ELD keeps the logical path for lookup diagnostics. For example, a failed
attempt to link `/missing.o` with `--chroot /sdk` reports `/missing.o`, and
verbose search messages show the logical search path rather than the chrooted path
`/sdk/missing.o`.

After a file has been resolved, lower-level I/O, parsing, and loading code uses
the resolved path. Diagnostics from those stages can therefore include the
chrooted path.

When creating a reproduce archive, ELD captures the resolved files and writes
a replayable command line without `--chroot`, same as how `-L` and `--sysroot`
options are skipped in the reproducer response file.
