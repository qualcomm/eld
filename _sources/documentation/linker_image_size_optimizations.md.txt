# Linker Image Size Optimizations

## Merging constants

ELD can merge identical constant data in mergeable, non-string input sections.
These sections are commonly emitted as `.rodata.cst*` sections with the
`SHF_MERGE` flag. Constant merging reduces duplicate bytes in `.rodata` and
related mergeable constant sections while preserving references to the data.

At a high level, ELD:

- identifies identical constant payloads,
- respects merge semantics such as entry size and alignment,
- keeps one canonical constant payload, and
- retargets eligible merge-kind relocations to that canonical payload.

### Alignment compatibility

An identical payload is merged only when its input offset and section
alignment can satisfy the same output address. For an input offset `O` in a
section aligned to `A`, the output address `X` must satisfy:

```text
X = O (mod A)
```

Two occurrences are compatible only when:

```text
(O1 - O2) is divisible by gcd(A1, A2)
```

For example, a 4-byte constant at offset `0` in an 8-byte-aligned section and
the same constant at offset `4` in a 16-byte-aligned section cannot be merged:

```text
b.o: X = 0 (mod 8)
c.o: X = 4 (mod 16)
```

Here, `gcd(8, 16) = 8`, but `0 - 4` is not divisible by `8`. The first
occurrence requires an 8-byte-aligned address, while the second requires an
address 4 bytes past a 16-byte boundary, so ELD keeps both copies. In
contrast, offsets `0` and `8` with the same section alignments are compatible
and may share one output address.

### Interaction with garbage collection

Garbage collection runs before constant merging in the normal final-link flow.
Consequently, discarded input sections do not contribute candidates, and only
live sections participate in canonicalization. Enabling `--gc-sections` can
therefore reduce both the direct data size and the set of constants available
for merging.

### Map-file representation

In text map files, merged constants are represented by a surviving canonical
input-section entry followed by comment lines for merged contributors. For
example:

```text
.rodata.cst4   <out_off> <size> <file2.o> #SHT_PROGBITS,SHF_ALLOC|SHF_MERGE,4
  # .rodata.cst4 0x0 <file1.o>
```

The non-comment line identifies the canonical contributor retained in the
layout. Comment-prefixed entries identify input sections merged into that
storage. See [linker map files](linker_map_files.md) for general map-file
navigation.

### Disabling constant merging

Use `--no-merge-constants` to disable canonicalization of mergeable constant
data. This can be useful when debugging layout or address-sensitive issues, or
when measuring the size contribution of constant merging independently.

Example A/B comparison:

```text
ld.eld foo.o bar.o -o app.default.elf
ld.eld --no-merge-constants foo.o bar.o -o app.nomergeconst.elf
readelf -S -W app.default.elf app.nomergeconst.elf
```
