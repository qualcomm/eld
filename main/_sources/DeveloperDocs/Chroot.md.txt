# Chroot support

`--chroot <directory>` makes ELD resolve absolute input paths beneath
`<directory>`. For example, with `--chroot /sdk`, an input path of
`/usr/lib/libfoo.a` is read from `/sdk/usr/lib/libfoo.a`. The chroot path prefix
is not applied to the output paths such as the output file path specified using `-o`,
or the map-file `-Map`.

The key motivation for adding this feature is to support lld reproducers,
which make use of the `--chroot` option. `--chroot <directory>` is a hidden feature
and should not be used outside this use case.

It applies when resolving normal input files, linker scripts, their INCLUDEs,
and explicit plugin configuration paths. Search-directory discovery and plugin
library lookups use host paths instead, as described below.
**Relative paths are always used unchanged, and output paths are not remapped.**

This is linker path remapping, not the Linux `chroot(2)` system call. It does
not change the process root or provide isolation.

## Chroot scope

All `SearchDirs` operations probe and return host paths without applying
chroot. This includes `-L` and `SEARCH_DIR` searches, direct plugin library paths,
linker-install plugin defaults, `-rpath` (including `$ORIGIN`, `@loader_path`, and
`@executable_path`), environment-library paths, and current-directory searches.
Search order and sysroot expansion are unchanged.

Inputs found through `-L` or `SEARCH_DIR` are read from their host paths without
subsequent chroot remapping. This includes libraries, linker scripts, and
`INCLUDE` searches. For example, `--chroot /sdk -L /usr/lib -lfoo` reads
`/usr/lib/libfoo.a`, not `/sdk/usr/lib/libfoo.a`. A library present only beneath
the chroot is not discovered by that search. This differs from lld, which
applies chroot when opening an absolute path selected by its host-only search.

Plugin libraries use the host paths returned by `SearchDirs` directly, without
subsequent chroot remapping.

## Plugin configuration files

Absolute paths passed to `--plugin-config` are rooted before checking whether
the file exists. For example, `--chroot /sdk --plugin-config /plugins/config.yaml`
reads `/sdk/plugins/config.yaml`, not the host `/plugins/config.yaml`. Relative
configuration paths are unchanged. A missing rooted absolute config is an error;
it does not fall back to host search paths.

The same rule applies to direct paths passed to the plugin API
`LinkerWrapper::findConfigFile`. Its returned path is already resolved and must
not be rooted again when reading the file. During replay, an existing config
mapping is used before attempting a chrooted lookup.

Configuration files discovered through `-L` or the linker-install defaults
still use host paths, without subsequent chroot remapping. Paths to plugin
libraries and opaque plugin options are not remapped.

Reproduce archives capture the rooted configuration files and can be replayed
without the original files or `--chroot`.

## Interaction with linker-script rules

File patterns in linker scripts match the logical input path, before
`--chroot` adds its host prefix. For example, `/foo.o(.text)` matches an input
opened from `/sdk/foo.o` when the command line specifies `--chroot /sdk` and
`/foo.o`. The chroot directory is not part of the path used for the linker-script rule matching.

## Interaction with `--remap-inputs`

ELD applies `--remap-inputs` before `--chroot`. A remapped absolute path is
therefore resolved beneath the chroot as expected. For example:

```bash
--remap-inputs=/build/foo.o=/cache/foo.o --chroot /sdk /build/foo.o
```

Linker reads `/sdk/cache/foo.o`.

## Interaction with `--sysroot`

ELD expands `=` and `$SYSROOT` path markers using `--sysroot` before applying
`--chroot`. For example:

```text
--sysroot /toolchain --chroot /sdk =/usr/lib/libfoo.a
```

reads `/sdk/toolchain/usr/lib/libfoo.a`. For search directories such as
`-L=/usr/lib`, sysroot expansion happens during host discovery, and the selected
input is read without applying chroot.

## Diagnostics

ELD keeps the logical path for normal input-file lookup diagnostics. For example,
a failed attempt to link `/missing.o` with `--chroot /sdk` reports `/missing.o`, and
verbose search messages show the logical search path rather than the chrooted path
`/sdk/missing.o`.

Plugin-config search messages can show the rooted configuration path.

After a file has been resolved, lower-level I/O, parsing, and loading code may use
the resolved path. Diagnostics from those stages can therefore include the
chrooted path.

When creating a reproduce archive, ELD captures the resolved files and writes
a replayable command line without `--chroot`, same as how `-L` and `--sysroot`
options are skipped in the reproducer response file.
