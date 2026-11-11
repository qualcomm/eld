# i386 (x86-32) Relocation Reference

## Introduction

This document describes the i386 ELF32 relocations currently handled by ELD,
including their encoding types, implicit-addend rules, operations, range
checks, and GOT-base behaviour.

The System V i386 ABI is the reference ABI for the target:
https://gitlab.com/x86-psABIs/i386-ABI

---

## GOT/PLT Allocation Annotations

Modes: `S` = static executable, `D` = dynamic executable, `P` = PIE,
`H` = shared library; `I` overrides `P` for static PIE.
Resources: `G` = symbol GOT slot, `J` = PLT destination GOT slot,
`L` = PLT entry, `R` = IFUNC resolver PLT; `-` = no allocation.
Conditions: `p` = Preemptible=true, `f` = function, `i` = IFUNC,
`g` = global-bound, `t` = TLS, `h` = local-bound hidden.
`[a&b]` requires both conditions; `!` means NOT. Unconditional entries
apply to any visibility/type. TLS resources, modifiers, and examples are
in the [shared annotation legend](Relocation-Reservation-Annotations.md#allocation-annotations);
see also the [preemption flowchart](Relocation-Reservation-Annotations.md#visibility-and-preemption).

- The supported relocation set has **no per-symbol GOT or PLT
  reservation** path, including for `STT_FUNC`, `STT_GNU_IFUNC`, or `STT_TLS`.
  ABI relocation names alone do not imply dynamic i386 support in ELD.
- `GOTOFF`/`GOTPC` materialize the 12-byte `GOTPLT0` base for any
  visibility/type in every final-link mode, not an imported symbol's
  individual GOT entry or a function PLT entry.
- No range trampoline is registered. Narrow PC-relative overflow retains
  its range policy; full-width ELF32 wraparound does not create a stub.

Sources: [scanner](../../lib/Target/X86/X86_32Relocator.cpp),
[backend](../../lib/Target/X86/X86_32LDBackend.cpp).

## Relocation Conventions

| Symbol | Meaning |
|--------|---------|
| `S` | Runtime address of the referenced symbol |
| `A` | Addend read from the relocation field in an ELF32 `Rel` entry |
| `P` | Address of the relocation site |
| `GOT` | Address of the linker-defined i386 GOT base |
| `GOTPLT0` | The 12-byte i386 initial GOT/PLT fragment used as the GOT base |
| `X` | Computed relocation result before field encoding |
| `X32` | `X` normalized to the low 32 bits of the ELF32 result |

i386 uses `Elf32_Rel` records rather than `Elf32_Rela` records. An input
relocation therefore has no explicit `r_addend` field. ELD reads the addend
from the bytes already present at the relocation target, sign-extending it
from the relocation field width, and then applies the relocation formula.
For normal ELF32 `Rel` input the generic relocation addend is zero; the
effective addend is the field value plus any generic addend supplied by ELD.

---

## Encoding Types

i386 uses flat-field encodings. The supported ELD relocations write the low
bits of the computed result directly into the relocation field.

| Encoding | Value bits written | Field size |
|----------|--------------------|------------|
| `EncTy_None` | none | — |
| `EncTy_8` | `X32[7:0]` → field `[7:0]` | 8-bit |
| `EncTy_16` | `X32[15:0]` → field `[15:0]` | 16-bit |
| `EncTy_32` | `X32[31:0]` → field `[31:0]` | 32-bit |

---

## Range Checks

ELD first normalizes every computed result to the ELF32 address space:

```text
X32 = X & 0xffffffff
SignedX32 = sign_extend(X32, 32)
```

The normalized value is then used for narrow-field range checks and for the
final write, including when the arithmetic result crosses the 32-bit
boundary.

The range policies are:

* Full-width 32-bit relocations accept any 32-bit bit pattern. No overflow
  check is needed because the field is the full ELF32 word.
* `R_386_8` and `R_386_16` accept a result representable as either a signed
  or an unsigned field.
* `R_386_PC8` requires a signed 8-bit result: `-128 ≤ X ≤ 127`.
* `R_386_PC16` accepts a signed 17-bit result:
  `-65536 ≤ X ≤ 65535`. The accepted result is then truncated to 16 bits.
  This accommodates the wraparound used by real-mode code.

The 8- and 16-bit i386 relocation forms are extensions rather than part of
the usual i386 psABI relocation set. They are required by the current
real-mode and setup-code inputs.

| Relocation | Encoding | Range policy | Effective bound | GOT allocation | PLT allocation | Trampoline / range |
|------------|----------|--------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_386_NONE` | `EncTy_None` | none | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_386_32` | `EncTy_32` | none | any 32-bit bit pattern | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_386_16` | `EncTy_16` | signed or unsigned | signed 16-bit or unsigned 16-bit | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_386_8` | `EncTy_8` | signed or unsigned | signed 8-bit or unsigned 8-bit | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_386_PC32` | `EncTy_32` | none | any 32-bit bit pattern | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_386_PC16` | `EncTy_16` | signed 17-bit | `-65536 ≤ X ≤ 65535` | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_386_PC8` | `EncTy_8` | signed 8-bit | `-128 ≤ X ≤ 127` | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_386_GOTOFF` | `EncTy_32` | none | any 32-bit bit pattern | <ul><li><code>S: B</code></li><li><code>D: B</code></li><li><code>P: B</code></li><li><code>H: B</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_386_GOTPC` | `EncTy_32` | none | any 32-bit bit pattern | <ul><li><code>S: B</code></li><li><code>D: B</code></li><li><code>P: B</code></li><li><code>H: B</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |


---

## Relocation Table

### Absolute Relocations

The computed value `S + A` is written into the relocation site. Because i386
uses `Elf32_Rel`, `A` is read from the original contents of that site.

| Relocation | Type | Encoding | Operation | Range check | GOT allocation | PLT allocation | Trampoline / range |
|------------|------|----------|-----------|-------------|-------------------------|-------------------------|--------------------|
| `R_386_NONE` | 0 | `EncTy_None` | — | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_386_32` | 1 | `EncTy_32` | `S + A` | none; write the ELF32 result | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_386_16` | 20 | `EncTy_16` | `S + A` | signed or unsigned 16-bit | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_386_8` | 22 | `EncTy_8` | `S + A` | signed or unsigned 8-bit | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |

### PC-Relative Relocations

The computed value `S + A - P` is written into the relocation site. `P` is
the address of the field being relocated.

| Relocation | Type | Encoding | Operation | Range check | GOT allocation | PLT allocation | Trampoline / range |
|------------|------|----------|-----------|-------------|-------------------------|-------------------------|--------------------|
| `R_386_PC32` | 2 | `EncTy_32` | `S + A - P` | none; write the ELF32 result | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_386_PC16` | 21 | `EncTy_16` | `S + A - P` | signed 17-bit before truncation | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_386_PC8` | 23 | `EncTy_8` | `S + A - P` | signed 8-bit | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |

### GOT-Base Relocations

These relocations use the address of the linker-created i386 GOT base. They
do not allocate an imported symbol's individual GOT entry.

| Relocation | Type | Encoding | Operation | Range check | GOT allocation | PLT allocation | Trampoline / range |
|------------|------|----------|-----------|-------------|-------------------------|-------------------------|--------------------|
| `R_386_GOTOFF` | 9 | `EncTy_32` | `S + A - GOT` | none; write the ELF32 result | <ul><li><code>S: B</code></li><li><code>D: B</code></li><li><code>P: B</code></li><li><code>H: B</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_386_GOTPC` | 10 | `EncTy_32` | `GOT + A - P` | none; write the ELF32 result | <ul><li><code>S: B</code></li><li><code>D: B</code></li><li><code>P: B</code></li><li><code>H: B</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |

When either relocation is scanned, ELD materializes the i386 `GOTPLT0`
fragment and defines the hidden `_GLOBAL_OFFSET_TABLE_` symbol. After layout,
both relocations use the address of that fragment as `GOT`.

The i386 `GOTPLT0` fragment is 12 bytes:

| Bytes | Content |
|-------|---------|
| 0–3 | Address of `.dynamic` |
| 4–11 | Reserved bytes, currently zero |

---

## Common Relocation Encodings

### Absolute Address

```asm
.long 0
.reloc 0, R_386_32, symbol       # S + A; 32-bit result
```

### Narrow Absolute Address

```asm
.byte 0
.reloc 0, R_386_8, symbol        # S + A; signed or unsigned 8-bit

.word 0
.reloc 1, R_386_16, symbol       # S + A; signed or unsigned 16-bit
```

### PC-Relative Address

```asm
.long 0
.reloc 0, R_386_PC32, symbol     # S + A - P
```

The same `S + A - P` formula is used by `R_386_PC8` and `R_386_PC16`, with
their respective range policies.

### GOT-Base Relocations

```asm
.long 0
.reloc 0, R_386_GOTOFF, symbol   # S + A - GOT

.long 0
.reloc 0, R_386_GOTPC, _GLOBAL_OFFSET_TABLE_  # GOT + A - P
```

---

## References

* System V i386 ABI specification:
  https://gitlab.com/x86-psABIs/i386-ABI
* LLVM i386 relocation definitions:
  https://github.com/llvm/llvm-project/blob/main/llvm/include/llvm/BinaryFormat/ELFRelocs/i386.def
