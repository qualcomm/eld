# Hexagon Relocation Reference

## Introduction

Hexagon is a VLIW DSP architecture where all instructions are 32 bits wide. Relocations encode symbol references into instruction immediate fields. The Hexagon ABI supports an extended relocation pair mechanism for large offsets: a non-extended relocation encodes the low 6 bits, while a paired `_X`-suffix relocation encodes the upper bits of the value.

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

- `GOT_32_6_X` and `GOT_11_X` allocate symbol-address slots for local
  and global targets. Other ordinary `GOT_*` variants allocate them only
  for non-local targets; their local-symbol scanner has no matching case.
  `GOTREL_*` creates only the shared GOT base, not a target-symbol slot.
  The local scanner skips hidden targets except for `GOT_32_6_X` and
  `GOT_11_X`; the `h` condition identifies these exceptions.
- Absolute/GP-relative function references use `Preemptible=true` and `STT_FUNC`.
  Branch/PC-relative references use `Preemptible=true` only in non-static links,
  including extended branches and `32_PCREL`/`6_PCREL_X`, without a
  function-type test for PLT allocation.
- Static weak-undefined branch references can target
  `__linker_guard_weak_undef` (unless disabled), not a PLT. Extended
  branches qualify for this guard only with `STT_FUNC`.
- Range stubs check signed `D >> 2`, with `D = target + A - P`.
  `B22_PCREL`, `PLT_B22_PCREL`, and non-extended GD/LD PLT calls have
  ±8 MiB reach; `B15_PCREL` ±64 KiB, `B13_PCREL` ±16 KiB, and
  `B9_PCREL` ±1 KiB. **`B7_PCREL` has no range trampoline**, despite
  its finite range. Extended `_X` forms do not request range stubs.
  Range testing does not require `STT_FUNC`; the target can be a selected
  PLT address, and PIC/non-PIC templates differ.
- TLS GD GOT references create a GD pair in dynamic links, or an IE
  slot plus a GD-to-IE access stub in static links. LD shares the module
  slot; IE reserves a slot in every mode. These require `STT_TLS`.
- GD/LD PLT-call relocations are retargeted: static links use GD-to-IE /
  LD-to-LE access stubs; dynamic links reserve a PLT for a resolver stub,
  **not the original TLS symbol**. Access stubs are model-driven, not
  distance-driven. Non-extended calls can additionally need range stubs.

Sources: [scanner / TLS creation](../../lib/Target/Hexagon/HexagonRelocator.cpp),
[range stub](../../lib/Target/Hexagon/HexagonAbsoluteStub.cpp),
[TLS stub](../../lib/Target/Hexagon/HexagonTLSStub.cpp).

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

| Relocation | Expression | EffectiveBits | Shift | Signed | Range check | Alignment check | GOT allocation | PLT allocation | Trampoline / range |
|------------|-----------|--------------|-------|--------|-------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_HEX_32` | `S + A` | 32 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_HEX_16` | `S + A` | 16 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_HEX_8` | `S + A` | 8 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_HEX_LO16` | `S + A` | 16 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_HEX_HI16` | `S + A` | 16 | 16 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |

`R_HEX_LO16` encodes bits `[15:0]` and `R_HEX_HI16` encodes bits `[31:16]` of `S + A` into an instruction immediate. `R_HEX_HL16` is not supported by ELD.

---

## PC-Relative Branch Relocations

Expression: `S + A - P`

### Non-Extended

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check | GOT allocation | PLT allocation | Trampoline / range |
|------------|--------------|-------|--------|-------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_HEX_B22_PCREL` | 22 | 2 | yes | `-2^23 ≤ X < 2^23` | `X % 4 == 0` | <ul><li><code>S: -</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: -</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: -</code></li><li><code>H: L[p]</code></li></ul> | Yes; outside [-2^23, 2^23) |
| `R_HEX_B15_PCREL` | 15 | 2 | yes | `-2^16 ≤ X < 2^16` | `X % 4 == 0` | <ul><li><code>S: -</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: -</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: -</code></li><li><code>H: L[p]</code></li></ul> | Yes; outside [-2^16, 2^16) |
| `R_HEX_B13_PCREL` | 13 | 2 | yes | `-2^14 ≤ X < 2^14` | `X % 4 == 0` | <ul><li><code>S: -</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: -</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: -</code></li><li><code>H: L[p]</code></li></ul> | Yes; outside [-2^14, 2^14) |
| `R_HEX_B9_PCREL` | 9 | 2 | yes | `-2^10 ≤ X < 2^10` | `X % 4 == 0` | <ul><li><code>S: -</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: -</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: -</code></li><li><code>H: L[p]</code></li></ul> | Yes; outside [-2^10, 2^10) |
| `R_HEX_B7_PCREL` | 7 | 2 | yes | `-2^8 ≤ X < 2^8` | `X % 4 == 0` | <ul><li><code>S: -</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: -</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: -</code></li><li><code>H: L[p]</code></li></ul> | No |
| `R_HEX_32_PCREL` | 32 | 0 | yes | — | — | <ul><li><code>S: -</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: -</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: -</code></li><li><code>H: L[p]</code></li></ul> | No |
| `R_HEX_6_PCREL_X` | 6 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: -</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: -</code></li><li><code>H: L[p]</code></li></ul> | No |

`R_HEX_32_PCREL` is a 32-bit PC-relative relocation used in data (not in instruction immediates). `R_HEX_6_PCREL_X` encodes the low 6 bits of an extended PC-relative pair.

### Extended (_X variants — upper bits of a two-relocation sequence)

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check | GOT allocation | PLT allocation | Trampoline / range |
|------------|--------------|-------|--------|-------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_HEX_B32_PCREL_X` | 26 | 6 | yes | — | — | <ul><li><code>S: -</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: -</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: -</code></li><li><code>H: L[p]</code></li></ul> | No |
| `R_HEX_B22_PCREL_X` | 22 | 0 | yes | `-2^21 ≤ X < 2^21` | — | <ul><li><code>S: -</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: -</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: -</code></li><li><code>H: L[p]</code></li></ul> | No |
| `R_HEX_B15_PCREL_X` | 15 | 0 | yes | `-2^14 ≤ X < 2^14` | — | <ul><li><code>S: -</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: -</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: -</code></li><li><code>H: L[p]</code></li></ul> | No |
| `R_HEX_B13_PCREL_X` | 13 | 0 | yes | `-2^12 ≤ X < 2^12` | — | <ul><li><code>S: -</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: -</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: -</code></li><li><code>H: L[p]</code></li></ul> | No |
| `R_HEX_B9_PCREL_X` | 9 | 0 | yes | `-2^8 ≤ X < 2^8` | — | <ul><li><code>S: -</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: -</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: -</code></li><li><code>H: L[p]</code></li></ul> | No |
| `R_HEX_B7_PCREL_X` | 7 | 0 | yes | `-2^6 ≤ X < 2^6` | — | <ul><li><code>S: -</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: -</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: -</code></li><li><code>H: L[p]</code></li></ul> | No |

#### Example: Extended B22 Pair

```asm
; Upper bits: R_HEX_B32_PCREL_X encodes (S+A-P)[31:6]
; Lower bits: R_HEX_6_PCREL_X encodes (S+A-P)[5:0]
call target
```

---

## GP-Relative Relocations

Expression: `S + A - GP`

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check | GOT allocation | PLT allocation | Trampoline / range |
|------------|--------------|-------|--------|-------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_HEX_GPREL16_0` | 16 | 0 | no | `0 ≤ X < 2^16` | `X % 1 == 0` (any) | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_HEX_GPREL16_1` | 16 | 1 | no | `0 ≤ X < 2^17` | `X % 2 == 0` | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_HEX_GPREL16_2` | 16 | 2 | no | `0 ≤ X < 2^18` | `X % 4 == 0` | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_HEX_GPREL16_3` | 16 | 3 | no | `0 ≤ X < 2^19` | `X % 8 == 0` | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |

The suffix `_0` through `_3` reflects the access size: byte, halfword, word, and doubleword respectively.

---

## Absolute Extended/Truncated Relocations

Expression: `S + A`

`R_HEX_32_6_X` encodes bits `[31:6]` into a 26-bit instruction field (shift 6). The `_X` variants encode only the low 6 bits of the value with no shift, forming the lower half of an absolute extended pair.

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check | GOT allocation | PLT allocation | Trampoline / range |
|------------|--------------|-------|--------|-------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_HEX_32_6_X` | 26 | 6 | no | `0 ≤ X < 2^32` | — | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_HEX_16_X` | 6 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_HEX_12_X` | 6 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_HEX_11_X` | 6 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_HEX_10_X` | 6 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_HEX_9_X` | 6 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_HEX_8_X` | 6 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_HEX_7_X` | 6 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_HEX_6_X` | 6 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |

---

## Dynamic Relocations

These relocations are resolved by the dynamic linker at load time.

| Relocation | Purpose | GOT allocation | PLT allocation | Trampoline / range |
|------------|---------|-------------------------|-------------------------|--------------------|
| `R_HEX_COPY` | Copy relocation — copy symbol from shared library to executable's BSS | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GLOB_DAT` | Fill a GOT entry with the symbol's runtime address | <ul><li><code>S: U</code></li><li><code>D: U</code></li><li><code>P: U</code></li><li><code>H: U</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_JMP_SLOT` | PLT stub — dynamic linker fills with function address | <ul><li><code>S: U</code></li><li><code>D: U</code></li><li><code>P: U</code></li><li><code>H: U</code></li></ul> | <ul><li><code>S: U</code></li><li><code>D: U</code></li><li><code>P: U</code></li><li><code>H: U</code></li></ul> | No |
| `R_HEX_RELATIVE` | Base-relative relocation — add load bias to a stored address | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |

---

## GOT-Relative Relocations

### GOT-offset (Expression: `GOT(S) + A - GOT_ORG`)

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check | GOT allocation | PLT allocation | Trampoline / range |
|------------|--------------|-------|--------|-------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_HEX_GOTREL_LO16` | 16 | 0 | yes | — | — | <ul><li><code>S: B[!h]</code></li><li><code>D: B[!h]</code></li><li><code>P: B[!h]</code></li><li><code>H: B[!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GOTREL_HI16` | 16 | 16 | yes | — | — | <ul><li><code>S: B[!h]</code></li><li><code>D: B[!h]</code></li><li><code>P: B[!h]</code></li><li><code>H: B[!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GOTREL_32` | 32 | 0 | yes | — | — | <ul><li><code>S: B[!h]</code></li><li><code>D: B[!h]</code></li><li><code>P: B[!h]</code></li><li><code>H: B[!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GOTREL_32_6_X` | 26 | 6 | yes | — | — | <ul><li><code>S: B[!h]</code></li><li><code>D: B[!h]</code></li><li><code>P: B[!h]</code></li><li><code>H: B[!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GOTREL_16_X` | 6 | 0 | no | — | — | <ul><li><code>S: B[!h]</code></li><li><code>D: B[!h]</code></li><li><code>P: B[!h]</code></li><li><code>H: B[!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GOTREL_11_X` | 6 | 0 | no | — | — | <ul><li><code>S: B[!h]</code></li><li><code>D: B[!h]</code></li><li><code>P: B[!h]</code></li><li><code>H: B[!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |

### GOT entry PC-relative (Expression: `GOT(S) + A - P`)

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check | GOT allocation | PLT allocation | Trampoline / range |
|------------|--------------|-------|--------|-------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_HEX_GOT_LO16` | 16 | 0 | yes | — | — | <ul><li><code>S: G[g]</code></li><li><code>D: G[g]</code></li><li><code>P: G[g]</code></li><li><code>H: G[g]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GOT_HI16` | 16 | 16 | yes | — | — | <ul><li><code>S: G[g]</code></li><li><code>D: G[g]</code></li><li><code>P: G[g]</code></li><li><code>H: G[g]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GOT_32` | 32 | 0 | yes | — | — | <ul><li><code>S: G[g]</code></li><li><code>D: G[g]</code></li><li><code>P: G[g]</code></li><li><code>H: G[g]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GOT_16` | 16 | 0 | yes | `-2^15 ≤ X < 2^15` | — | <ul><li><code>S: G[g]</code></li><li><code>D: G[g]</code></li><li><code>P: G[g]</code></li><li><code>H: G[g]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GOT_32_6_X` | 26 | 6 | yes | — | — | <ul><li><code>S: G</code></li><li><code>D: G</code></li><li><code>P: G</code></li><li><code>H: G</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GOT_16_X` | 6 | 0 | yes | — | — | <ul><li><code>S: G[g]</code></li><li><code>D: G[g]</code></li><li><code>P: G[g]</code></li><li><code>H: G[g]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GOT_11_X` | 6 | 0 | no | — | — | <ul><li><code>S: G</code></li><li><code>D: G</code></li><li><code>P: G</code></li><li><code>H: G</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |

---

## TLS Relocations

### General Dynamic (GD) and Local Dynamic (LD)

These relocations use a GOT-entry-based indirection. Expression: `GOT(S) + A - P`.

#### GD PLT Call

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check | GOT allocation | PLT allocation | Trampoline / range |
|------------|--------------|-------|--------|-------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_HEX_GD_PLT_B22_PCREL` | 22 | 2 | yes | `-2^23 ≤ X < 2^23` | `X % 4 == 0` | <ul><li><code>S: -{c}[t&amp;!h]</code></li><li><code>D: J[t&amp;!h]{z}</code></li><li><code>P: J[t&amp;!h]{z}; I: -{c}[t&amp;!h]</code></li><li><code>H: J[t&amp;!h]{z}</code></li></ul> | <ul><li><code>S: -{c}[t&amp;!h]</code></li><li><code>D: L[t&amp;!h]{z}</code></li><li><code>P: L[t&amp;!h]{z}; I: -{c}[t&amp;!h]</code></li><li><code>H: L[t&amp;!h]{z}</code></li></ul> | Yes; outside [-2^23, 2^23); TLS access stub |
| `R_HEX_GD_PLT_B22_PCREL_X` | 22 | 0 | yes | — | — | <ul><li><code>S: -{c}[t&amp;!h]</code></li><li><code>D: J[t&amp;!h]{z}</code></li><li><code>P: J[t&amp;!h]{z}; I: -{c}[t&amp;!h]</code></li><li><code>H: J[t&amp;!h]{z}</code></li></ul> | <ul><li><code>S: -{c}[t&amp;!h]</code></li><li><code>D: L[t&amp;!h]{z}</code></li><li><code>P: L[t&amp;!h]{z}; I: -{c}[t&amp;!h]</code></li><li><code>H: L[t&amp;!h]{z}</code></li></ul> | TLS access stub (all modes) |
| `R_HEX_GD_PLT_B32_PCREL_X` | 26 | 6 | yes | — | — | <ul><li><code>S: -{c}[t&amp;!h]</code></li><li><code>D: J[t&amp;!h]{z}</code></li><li><code>P: J[t&amp;!h]{z}; I: -{c}[t&amp;!h]</code></li><li><code>H: J[t&amp;!h]{z}</code></li></ul> | <ul><li><code>S: -{c}[t&amp;!h]</code></li><li><code>D: L[t&amp;!h]{z}</code></li><li><code>P: L[t&amp;!h]{z}; I: -{c}[t&amp;!h]</code></li><li><code>H: L[t&amp;!h]{z}</code></li></ul> | TLS access stub (all modes) |

#### GD GOT Access

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check | GOT allocation | PLT allocation | Trampoline / range |
|------------|--------------|-------|--------|-------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_HEX_GD_GOT_LO16` | 16 | 0 | yes | — | — | <ul><li><code>S: O[t&amp;!h]{c}</code></li><li><code>D: Q[t&amp;!h]</code></li><li><code>P: Q[t&amp;!h]; I: O[t&amp;!h]{c}</code></li><li><code>H: Q[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GD_GOT_HI16` | 16 | 16 | yes | — | — | <ul><li><code>S: O[t&amp;!h]{c}</code></li><li><code>D: Q[t&amp;!h]</code></li><li><code>P: Q[t&amp;!h]; I: O[t&amp;!h]{c}</code></li><li><code>H: Q[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GD_GOT_32` | 32 | 0 | yes | — | — | <ul><li><code>S: O[t&amp;!h]{c}</code></li><li><code>D: Q[t&amp;!h]</code></li><li><code>P: Q[t&amp;!h]; I: O[t&amp;!h]{c}</code></li><li><code>H: Q[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GD_GOT_16` | 16 | 0 | yes | `-2^15 ≤ X < 2^15` | `X % 4 == 0` | <ul><li><code>S: O[t&amp;!h]{c}</code></li><li><code>D: Q[t&amp;!h]</code></li><li><code>P: Q[t&amp;!h]; I: O[t&amp;!h]{c}</code></li><li><code>H: Q[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GD_GOT_32_6_X` | 26 | 6 | yes | — | — | <ul><li><code>S: O[t&amp;!h]{c}</code></li><li><code>D: Q[t&amp;!h]</code></li><li><code>P: Q[t&amp;!h]; I: O[t&amp;!h]{c}</code></li><li><code>H: Q[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GD_GOT_16_X` | 6 | 0 | no | — | — | <ul><li><code>S: O[t&amp;!h]{c}</code></li><li><code>D: Q[t&amp;!h]</code></li><li><code>P: Q[t&amp;!h]; I: O[t&amp;!h]{c}</code></li><li><code>H: Q[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_GD_GOT_11_X` | 6 | 0 | no | — | — | <ul><li><code>S: O[t&amp;!h]{c}</code></li><li><code>D: Q[t&amp;!h]</code></li><li><code>P: Q[t&amp;!h]; I: O[t&amp;!h]{c}</code></li><li><code>H: Q[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |

The LD variants follow the same structure:

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check | GOT allocation | PLT allocation | Trampoline / range |
|------------|--------------|-------|--------|-------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_HEX_LD_PLT_B22_PCREL` | 22 | 2 | yes | `-2^23 ≤ X < 2^23` | `X % 4 == 0` | <ul><li><code>S: -{c}[t&amp;!h]</code></li><li><code>D: J[t&amp;!h]{z}</code></li><li><code>P: J[t&amp;!h]{z}; I: -{c}[t&amp;!h]</code></li><li><code>H: J[t&amp;!h]{z}</code></li></ul> | <ul><li><code>S: -{c}[t&amp;!h]</code></li><li><code>D: L[t&amp;!h]{z}</code></li><li><code>P: L[t&amp;!h]{z}; I: -{c}[t&amp;!h]</code></li><li><code>H: L[t&amp;!h]{z}</code></li></ul> | Yes; outside [-2^23, 2^23); TLS access stub |
| `R_HEX_LD_PLT_B22_PCREL_X` | 22 | 0 | yes | — | — | <ul><li><code>S: -{c}[t&amp;!h]</code></li><li><code>D: J[t&amp;!h]{z}</code></li><li><code>P: J[t&amp;!h]{z}; I: -{c}[t&amp;!h]</code></li><li><code>H: J[t&amp;!h]{z}</code></li></ul> | <ul><li><code>S: -{c}[t&amp;!h]</code></li><li><code>D: L[t&amp;!h]{z}</code></li><li><code>P: L[t&amp;!h]{z}; I: -{c}[t&amp;!h]</code></li><li><code>H: L[t&amp;!h]{z}</code></li></ul> | TLS access stub (all modes) |
| `R_HEX_LD_PLT_B32_PCREL_X` | 26 | 6 | yes | — | — | <ul><li><code>S: -{c}[t&amp;!h]</code></li><li><code>D: J[t&amp;!h]{z}</code></li><li><code>P: J[t&amp;!h]{z}; I: -{c}[t&amp;!h]</code></li><li><code>H: J[t&amp;!h]{z}</code></li></ul> | <ul><li><code>S: -{c}[t&amp;!h]</code></li><li><code>D: L[t&amp;!h]{z}</code></li><li><code>P: L[t&amp;!h]{z}; I: -{c}[t&amp;!h]</code></li><li><code>H: L[t&amp;!h]{z}</code></li></ul> | TLS access stub (all modes) |
| `R_HEX_LD_GOT_LO16` | 16 | 0 | yes | — | — | <ul><li><code>S: N[t&amp;!h]</code></li><li><code>D: N[t&amp;!h]</code></li><li><code>P: N[t&amp;!h]</code></li><li><code>H: N[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_LD_GOT_HI16` | 16 | 16 | yes | — | — | <ul><li><code>S: N[t&amp;!h]</code></li><li><code>D: N[t&amp;!h]</code></li><li><code>P: N[t&amp;!h]</code></li><li><code>H: N[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_LD_GOT_32` | 32 | 0 | no | — | — | <ul><li><code>S: N[t&amp;!h]</code></li><li><code>D: N[t&amp;!h]</code></li><li><code>P: N[t&amp;!h]</code></li><li><code>H: N[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_LD_GOT_16` | 16 | 0 | no | `0 ≤ X < 2^16` | `X % 4 == 0` | <ul><li><code>S: N[t&amp;!h]</code></li><li><code>D: N[t&amp;!h]</code></li><li><code>P: N[t&amp;!h]</code></li><li><code>H: N[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_LD_GOT_32_6_X` | 26 | 6 | yes | — | — | <ul><li><code>S: N[t&amp;!h]</code></li><li><code>D: N[t&amp;!h]</code></li><li><code>P: N[t&amp;!h]</code></li><li><code>H: N[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_LD_GOT_16_X` | 6 | 0 | no | — | — | <ul><li><code>S: N[t&amp;!h]</code></li><li><code>D: N[t&amp;!h]</code></li><li><code>P: N[t&amp;!h]</code></li><li><code>H: N[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_LD_GOT_11_X` | 6 | 0 | no | — | — | <ul><li><code>S: N[t&amp;!h]</code></li><li><code>D: N[t&amp;!h]</code></li><li><code>P: N[t&amp;!h]</code></li><li><code>H: N[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |

### Initial Exec (IE)

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check | GOT allocation | PLT allocation | Trampoline / range |
|------------|--------------|-------|--------|-------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_HEX_IE_LO16` | 16 | 0 | yes | — | — | <ul><li><code>S: O[t&amp;!h]</code></li><li><code>D: O[t&amp;!h]</code></li><li><code>P: O[t&amp;!h]</code></li><li><code>H: O[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_IE_HI16` | 16 | 16 | yes | — | — | <ul><li><code>S: O[t&amp;!h]</code></li><li><code>D: O[t&amp;!h]</code></li><li><code>P: O[t&amp;!h]</code></li><li><code>H: O[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_IE_32` | 32 | 0 | yes | — | — | <ul><li><code>S: O[t&amp;!h]</code></li><li><code>D: O[t&amp;!h]</code></li><li><code>P: O[t&amp;!h]</code></li><li><code>H: O[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_IE_32_6_X` | 26 | 6 | yes | — | — | <ul><li><code>S: O[t&amp;!h]</code></li><li><code>D: O[t&amp;!h]</code></li><li><code>P: O[t&amp;!h]</code></li><li><code>H: O[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_IE_16_X` | 6 | 0 | no | — | — | <ul><li><code>S: O[t&amp;!h]</code></li><li><code>D: O[t&amp;!h]</code></li><li><code>P: O[t&amp;!h]</code></li><li><code>H: O[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_IE_GOT_LO16` | 16 | 0 | yes | — | — | <ul><li><code>S: O[t&amp;!h]</code></li><li><code>D: O[t&amp;!h]</code></li><li><code>P: O[t&amp;!h]</code></li><li><code>H: O[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_IE_GOT_HI16` | 16 | 16 | yes | — | — | <ul><li><code>S: O[t&amp;!h]</code></li><li><code>D: O[t&amp;!h]</code></li><li><code>P: O[t&amp;!h]</code></li><li><code>H: O[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_IE_GOT_32` | 32 | 0 | yes | — | — | <ul><li><code>S: O[t&amp;!h]</code></li><li><code>D: O[t&amp;!h]</code></li><li><code>P: O[t&amp;!h]</code></li><li><code>H: O[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_IE_GOT_32_6_X` | 26 | 6 | yes | — | — | <ul><li><code>S: O[t&amp;!h]</code></li><li><code>D: O[t&amp;!h]</code></li><li><code>P: O[t&amp;!h]</code></li><li><code>H: O[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_IE_GOT_16_X` | 6 | 0 | no | — | — | <ul><li><code>S: O[t&amp;!h]</code></li><li><code>D: O[t&amp;!h]</code></li><li><code>P: O[t&amp;!h]</code></li><li><code>H: O[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_IE_GOT_11_X` | 6 | 0 | no | — | — | <ul><li><code>S: O[t&amp;!h]</code></li><li><code>D: O[t&amp;!h]</code></li><li><code>P: O[t&amp;!h]</code></li><li><code>H: O[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_IE_GOT_16` | 16 | 0 | yes | `-2^15 ≤ X < 2^15` | `X % 4 == 0` | <ul><li><code>S: O[t&amp;!h]</code></li><li><code>D: O[t&amp;!h]</code></li><li><code>P: O[t&amp;!h]</code></li><li><code>H: O[t&amp;!h]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |

### Local Exec (LE) — TP-Relative

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check | GOT allocation | PLT allocation | Trampoline / range |
|------------|--------------|-------|--------|-------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_HEX_TPREL_LO16` | 16 | 0 | yes | — | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_TPREL_HI16` | 16 | 16 | yes | — | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_TPREL_32` | 32 | 0 | yes | — | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_TPREL_16` | 16 | 0 | yes | `-2^15 ≤ X < 2^15` | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_TPREL_32_6_X` | 26 | 6 | yes | — | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_TPREL_16_X` | 6 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_TPREL_11_X` | 6 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |

### DTPREL — Module-Relative (Local Dynamic)

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check | GOT allocation | PLT allocation | Trampoline / range |
|------------|--------------|-------|--------|-------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_HEX_DTPMOD_32` | — | — | — | — | — | <ul><li><code>S: U</code></li><li><code>D: U</code></li><li><code>P: U</code></li><li><code>H: U</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_DTPREL_LO16` | 16 | 0 | yes | — | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_DTPREL_HI16` | 16 | 16 | yes | — | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_DTPREL_32` | 32 | 0 | yes | — | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_DTPREL_16` | 16 | 0 | yes | `-2^15 ≤ X < 2^15` | `X % 4 == 0` | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_DTPREL_32_6_X` | 26 | 6 | yes | — | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_DTPREL_16_X` | 6 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_DTPREL_11_X` | 6 | 0 | no | — | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |

`R_HEX_DTPMOD_32` is a dynamic relocation; the dynamic linker fills it with the module ID.

---

## Register Relocations

These relocations encode an address into an instruction's register-index field.

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check | GOT allocation | PLT allocation | Trampoline / range |
|------------|--------------|-------|--------|-------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_HEX_23_REG` | 21 | 2 | no | `0 ≤ X < 2^23` | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_HEX_27_REG` | 25 | 2 | no | `0 ≤ X < 2^27` | — | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |

---

## PLT Relocations

| Relocation | EffectiveBits | Shift | Signed | Range check | Alignment check | GOT allocation | PLT allocation | Trampoline / range |
|------------|--------------|-------|--------|-------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_HEX_PLT_B22_PCREL` | 22 | 2 | yes | `-2^23 ≤ X < 2^23` | `X % 4 == 0` | <ul><li><code>S: -</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: -</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: -</code></li><li><code>H: L[p]</code></li></ul> | Yes; outside [-2^23, 2^23) |

`R_HEX_PLT_B22_PCREL` is used in PLT stubs to call the PLT resolver.

---

## References

- ELF for the Hexagon Architecture (Hexagon V73 ABI)
  - https://docs.qualcomm.com/bundle/publicresource/topics/80-N2040-1/ELF_for_the_Hexagon_Architecture.html
