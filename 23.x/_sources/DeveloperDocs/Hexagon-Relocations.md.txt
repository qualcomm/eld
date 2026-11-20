# Hexagon Relocation Reference

## Introduction

Hexagon is a VLIW DSP architecture where all instructions are 32 bits wide. Relocations encode symbol references into instruction immediate fields. The Hexagon ABI supports an extended relocation pair mechanism for large offsets: a non-extended relocation encodes the low 6 bits, while a paired `_X`-suffix relocation encodes the upper bits of the value.

## Relocation Conventions

| Symbol | Meaning |
|--------|---------|
| `S` | Runtime address of the referenced symbol |
| `A` | Relocation addend |
| `P` | Address of the relocation site (place) |
| `GP` | Global pointer value |
| `GOT` | Address of the Global Offset Table |
| `GOT(S)` | Address of the GOT entry for symbol `S` |
| `TP` | Thread pointer |

### Extended Relocation Pair Mechanism

For branch or absolute references that exceed the reach of a single immediate field, Hexagon uses a two-relocation sequence:

- **`R_HEX_B32_PCREL_X`** (or `R_HEX_32_6_X` for absolute) encodes bits `[31:6]` of the value into a 32-bit instruction immediate using a 26-bit field with shift 6.
- **`R_HEX_B*_PCREL_X` / `R_HEX_*_X`** (the non-extended counterpart) encodes bits `[5:0]` of the value into a 6-bit field with no shift.

Relocations with the `_X` suffix are the upper-bits half of such a pair. The lower-bits half uses `R_HEX_6_PCREL_X` (PC-relative) or a corresponding `R_HEX_*_X` variant (absolute).

### Table Columns

| Column | Meaning |
|--------|---------|
| EffectiveBits | Number of significant bits checked after the shift is applied |
| Shift | Right-shift applied to the value before encoding and before the range check |
| Signed | Whether the field is treated as a signed two's-complement value |
| Range check | Exact inequality the linker enforces on the raw value `X`; `—` means no check |
| Alignment check | Exact divisibility condition the linker enforces on the raw value `X`; `—` means no check |

**How the checks work.** Let `X` be the raw relocation value (e.g. `S+A-P`).
- Range (signed, N bits, shift S): `-(2^(N-1) × 2^S) ≤ X < 2^(N-1) × 2^S`  — i.e. `X >> S` fits in a signed N-bit field.
- Range (unsigned, N bits, shift S): `0 ≤ X < 2^N × 2^S` — i.e. `X >> S` fits in an unsigned N-bit field.
- Alignment: `X % A == 0` where `A` is the per-relocation alignment requirement shown in the check expression.

---

## Absolute Relocations

| Relocation | Expression | EffectiveBits | Shift | Signed | Range check | Alignment check |
|------------|-----------|--------------|-------|--------|-------------|-----------------|
| `R_HEX_32` | `S + A` | 32 | 0 | no | — | — |
| `R_HEX_16` | `S + A` | 16 | 0 | no | — | — |
| `R_HEX_8` | `S + A` | 8 | 0 | no | — | — |
| `R_HEX_LO16` | `S + A` | 16 | 0 | no | — | — |
| `R_HEX_HI16` | `S + A` | 16 | 16 | no | — | — |

`R_HEX_LO16` encodes bits `[15:0]` and `R_HEX_HI16` encodes bits `[31:16]` of `S + A` into an instruction immediate. `R_HEX_HL16` is not supported by ELD.

---

## PC-Relative Branch Relocations

Expression: `S + A - P`

### Non-Extended

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check |
|------------|--------------|-------|--------|-------------|-----------------|
| `R_HEX_B22_PCREL` | 22 | 2 | yes | `-2^23 ≤ X < 2^23` | `X % 4 == 0` |
| `R_HEX_B15_PCREL` | 15 | 2 | yes | `-2^16 ≤ X < 2^16` | `X % 4 == 0` |
| `R_HEX_B13_PCREL` | 13 | 2 | yes | `-2^14 ≤ X < 2^14` | `X % 4 == 0` |
| `R_HEX_B9_PCREL` | 9 | 2 | yes | `-2^10 ≤ X < 2^10` | `X % 4 == 0` |
| `R_HEX_B7_PCREL` | 7 | 2 | yes | `-2^8 ≤ X < 2^8` | `X % 4 == 0` |
| `R_HEX_32_PCREL` | 32 | 0 | yes | — | — |
| `R_HEX_6_PCREL_X` | 6 | 0 | no | — | — |

`R_HEX_32_PCREL` is a 32-bit PC-relative relocation used in data (not in instruction immediates). `R_HEX_6_PCREL_X` encodes the low 6 bits of an extended PC-relative pair.

### Extended (_X variants — upper bits of a two-relocation sequence)

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check |
|------------|--------------|-------|--------|-------------|-----------------|
| `R_HEX_B32_PCREL_X` | 26 | 6 | yes | — | — |
| `R_HEX_B22_PCREL_X` | 22 | 0 | yes | `-2^21 ≤ X < 2^21` | — |
| `R_HEX_B15_PCREL_X` | 15 | 0 | yes | `-2^14 ≤ X < 2^14` | — |
| `R_HEX_B13_PCREL_X` | 13 | 0 | yes | `-2^12 ≤ X < 2^12` | — |
| `R_HEX_B9_PCREL_X` | 9 | 0 | yes | `-2^8 ≤ X < 2^8` | — |
| `R_HEX_B7_PCREL_X` | 7 | 0 | yes | `-2^6 ≤ X < 2^6` | — |

#### Example: Extended B22 Pair

```asm
; Upper bits: R_HEX_B32_PCREL_X encodes (S+A-P)[31:6]
; Lower bits: R_HEX_6_PCREL_X encodes (S+A-P)[5:0]
call target
```

---

## GP-Relative Relocations

Expression: `S + A - GP`

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check |
|------------|--------------|-------|--------|-------------|-----------------|
| `R_HEX_GPREL16_0` | 16 | 0 | no | `0 ≤ X < 2^16` | `X % 1 == 0` (any) |
| `R_HEX_GPREL16_1` | 16 | 1 | no | `0 ≤ X < 2^17` | `X % 2 == 0` |
| `R_HEX_GPREL16_2` | 16 | 2 | no | `0 ≤ X < 2^18` | `X % 4 == 0` |
| `R_HEX_GPREL16_3` | 16 | 3 | no | `0 ≤ X < 2^19` | `X % 8 == 0` |

The suffix `_0` through `_3` reflects the access size: byte, halfword, word, and doubleword respectively.

---

## Absolute Extended/Truncated Relocations

Expression: `S + A`

`R_HEX_32_6_X` encodes bits `[31:6]` into a 26-bit instruction field (shift 6). The `_X` variants encode only the low 6 bits of the value with no shift, forming the lower half of an absolute extended pair.

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check |
|------------|--------------|-------|--------|-------------|-----------------|
| `R_HEX_32_6_X` | 26 | 6 | no | `0 ≤ X < 2^32` | — |
| `R_HEX_16_X` | 6 | 0 | no | — | — |
| `R_HEX_12_X` | 6 | 0 | no | — | — |
| `R_HEX_11_X` | 6 | 0 | no | — | — |
| `R_HEX_10_X` | 6 | 0 | no | — | — |
| `R_HEX_9_X` | 6 | 0 | no | — | — |
| `R_HEX_8_X` | 6 | 0 | no | — | — |
| `R_HEX_7_X` | 6 | 0 | no | — | — |
| `R_HEX_6_X` | 6 | 0 | no | — | — |

---

## Dynamic Relocations

These relocations are resolved by the dynamic linker at load time.

| Relocation | Purpose |
|------------|---------|
| `R_HEX_COPY` | Copy relocation — copy symbol from shared library to executable's BSS |
| `R_HEX_GLOB_DAT` | Fill a GOT entry with the symbol's runtime address |
| `R_HEX_JMP_SLOT` | PLT stub — dynamic linker fills with function address |
| `R_HEX_RELATIVE` | Base-relative relocation — add load bias to a stored address |

---

## GOT-Relative Relocations

### GOT-offset (Expression: `GOT(S) + A - GOT_ORG`)

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check |
|------------|--------------|-------|--------|-------------|-----------------|
| `R_HEX_GOTREL_LO16` | 16 | 0 | yes | — | — |
| `R_HEX_GOTREL_HI16` | 16 | 16 | yes | — | — |
| `R_HEX_GOTREL_32` | 32 | 0 | yes | — | — |
| `R_HEX_GOTREL_32_6_X` | 26 | 6 | yes | — | — |
| `R_HEX_GOTREL_16_X` | 6 | 0 | no | — | — |
| `R_HEX_GOTREL_11_X` | 6 | 0 | no | — | — |

### GOT entry PC-relative (Expression: `GOT(S) + A - P`)

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check |
|------------|--------------|-------|--------|-------------|-----------------|
| `R_HEX_GOT_LO16` | 16 | 0 | yes | — | — |
| `R_HEX_GOT_HI16` | 16 | 16 | yes | — | — |
| `R_HEX_GOT_32` | 32 | 0 | yes | — | — |
| `R_HEX_GOT_16` | 16 | 0 | yes | `-2^15 ≤ X < 2^15` | — |
| `R_HEX_GOT_32_6_X` | 26 | 6 | yes | — | — |
| `R_HEX_GOT_16_X` | 6 | 0 | yes | — | — |
| `R_HEX_GOT_11_X` | 6 | 0 | no | — | — |

---

## TLS Relocations

### General Dynamic (GD) and Local Dynamic (LD)

These relocations use a GOT-entry-based indirection. Expression: `GOT(S) + A - P`.

#### GD PLT Call

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check |
|------------|--------------|-------|--------|-------------|-----------------|
| `R_HEX_GD_PLT_B22_PCREL` | 22 | 2 | yes | `-2^23 ≤ X < 2^23` | `X % 4 == 0` |
| `R_HEX_GD_PLT_B22_PCREL_X` | 22 | 0 | yes | — | — |
| `R_HEX_GD_PLT_B32_PCREL_X` | 26 | 6 | yes | — | — |

#### GD GOT Access

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check |
|------------|--------------|-------|--------|-------------|-----------------|
| `R_HEX_GD_GOT_LO16` | 16 | 0 | yes | — | — |
| `R_HEX_GD_GOT_HI16` | 16 | 16 | yes | — | — |
| `R_HEX_GD_GOT_32` | 32 | 0 | yes | — | — |
| `R_HEX_GD_GOT_16` | 16 | 0 | yes | `-2^15 ≤ X < 2^15` | `X % 4 == 0` |
| `R_HEX_GD_GOT_32_6_X` | 26 | 6 | yes | — | — |
| `R_HEX_GD_GOT_16_X` | 6 | 0 | no | — | — |
| `R_HEX_GD_GOT_11_X` | 6 | 0 | no | — | — |

The LD variants follow the same structure:

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check |
|------------|--------------|-------|--------|-------------|-----------------|
| `R_HEX_LD_PLT_B22_PCREL` | 22 | 2 | yes | `-2^23 ≤ X < 2^23` | `X % 4 == 0` |
| `R_HEX_LD_PLT_B22_PCREL_X` | 22 | 0 | yes | — | — |
| `R_HEX_LD_PLT_B32_PCREL_X` | 26 | 6 | yes | — | — |
| `R_HEX_LD_GOT_LO16` | 16 | 0 | yes | — | — |
| `R_HEX_LD_GOT_HI16` | 16 | 16 | yes | — | — |
| `R_HEX_LD_GOT_32` | 32 | 0 | no | — | — |
| `R_HEX_LD_GOT_16` | 16 | 0 | no | `0 ≤ X < 2^16` | `X % 4 == 0` |
| `R_HEX_LD_GOT_32_6_X` | 26 | 6 | yes | — | — |
| `R_HEX_LD_GOT_16_X` | 6 | 0 | no | — | — |
| `R_HEX_LD_GOT_11_X` | 6 | 0 | no | — | — |

### Initial Exec (IE)

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check |
|------------|--------------|-------|--------|-------------|-----------------|
| `R_HEX_IE_LO16` | 16 | 0 | yes | — | — |
| `R_HEX_IE_HI16` | 16 | 16 | yes | — | — |
| `R_HEX_IE_32` | 32 | 0 | yes | — | — |
| `R_HEX_IE_32_6_X` | 26 | 6 | yes | — | — |
| `R_HEX_IE_16_X` | 6 | 0 | no | — | — |
| `R_HEX_IE_GOT_LO16` | 16 | 0 | yes | — | — |
| `R_HEX_IE_GOT_HI16` | 16 | 16 | yes | — | — |
| `R_HEX_IE_GOT_32` | 32 | 0 | yes | — | — |
| `R_HEX_IE_GOT_32_6_X` | 26 | 6 | yes | — | — |
| `R_HEX_IE_GOT_16_X` | 6 | 0 | no | — | — |
| `R_HEX_IE_GOT_11_X` | 6 | 0 | no | — | — |
| `R_HEX_IE_GOT_16` | 16 | 0 | yes | `-2^15 ≤ X < 2^15` | `X % 4 == 0` |

### Local Exec (LE) — TP-Relative

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check |
|------------|--------------|-------|--------|-------------|-----------------|
| `R_HEX_TPREL_LO16` | 16 | 0 | yes | — | — |
| `R_HEX_TPREL_HI16` | 16 | 16 | yes | — | — |
| `R_HEX_TPREL_32` | 32 | 0 | yes | — | — |
| `R_HEX_TPREL_16` | 16 | 0 | yes | `-2^15 ≤ X < 2^15` | — |
| `R_HEX_TPREL_32_6_X` | 26 | 6 | yes | — | — |
| `R_HEX_TPREL_16_X` | 6 | 0 | no | — | — |
| `R_HEX_TPREL_11_X` | 6 | 0 | no | — | — |

### DTPREL — Module-Relative (Local Dynamic)

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check |
|------------|--------------|-------|--------|-------------|-----------------|
| `R_HEX_DTPMOD_32` | — | — | — | — | — |
| `R_HEX_DTPREL_LO16` | 16 | 0 | yes | — | — |
| `R_HEX_DTPREL_HI16` | 16 | 16 | yes | — | — |
| `R_HEX_DTPREL_32` | 32 | 0 | yes | — | — |
| `R_HEX_DTPREL_16` | 16 | 0 | yes | `-2^15 ≤ X < 2^15` | `X % 4 == 0` |
| `R_HEX_DTPREL_32_6_X` | 26 | 6 | yes | — | — |
| `R_HEX_DTPREL_16_X` | 6 | 0 | no | — | — |
| `R_HEX_DTPREL_11_X` | 6 | 0 | no | — | — |

`R_HEX_DTPMOD_32` is a dynamic relocation; the dynamic linker fills it with the module ID.

---

## Register Relocations

These relocations encode an address into an instruction's register-index field.

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check |
|------------|--------------|-------|--------|-------------|-----------------|
| `R_HEX_23_REG` | 21 | 2 | no | `0 ≤ X < 2^23` | — |
| `R_HEX_27_REG` | 25 | 2 | no | `0 ≤ X < 2^27` | — |

---

## PLT Relocations

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check |
|------------|--------------|-------|--------|-------------|-----------------|
| `R_HEX_PLT_B22_PCREL` | 22 | 2 | yes | `-2^23 ≤ X < 2^23` | `X % 4 == 0` |

`R_HEX_PLT_B22_PCREL` is used in PLT stubs to call the PLT resolver.

---

## References

- ELF for the Hexagon Architecture (Hexagon V73 ABI)
  - https://docs.qualcomm.com/bundle/publicresource/topics/80-N2040-1/ELF_for_the_Hexagon_Architecture.html
