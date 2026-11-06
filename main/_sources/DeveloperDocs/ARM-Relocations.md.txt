# ARM Relocation Reference

## Introduction

ARM (32-bit) supports two instruction set architectures: the 32-bit ARM ISA and the 16/32-bit Thumb/Thumb-2 ISA. Relocations encode symbol references into instruction immediate fields and, for branch relocations, into the encoded offset. The linker selects ARM or Thumb encoding based on whether the target symbol is a Thumb function (the T bit).

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

- In relocation expressions `T` is the **Thumb-state bit**; allocation
  condition `t` denotes an ELF TLS symbol, not Thumb state. Veneer ISA selection
  uses the target address's low bit (or the selected PLT address).
- `GOT_BREL`/`GOT_PREL` reserve symbol slots for any visibility, symbol type, or link mode. TLS GD
  reserves a pair, LD shares a module slot, and IE reserves one slot,
  including in static/executable links (`STT_TLS`).
- Absolute function-address references use `Preemptible=true` and `STT_FUNC`. The branch
  scanner, including `PREL31`, uses `Preemptible=true` and additionally excludes
  hidden targets; it does not require `STT_FUNC`. Other PC-relative forms
  do not independently reserve an ordinary PLT.
- Static IFUNC address/GOT/branch references use the static indirect-function path; mixed
  direct/GOT references are tracked for final processing. Invalid IFUNC
  forms can warn and still allocate a PLT. Unsupported input relocations
  marked `N/A` are not usable even if scanning happens to allocate entries.
- ARM veneers check `D = target + A + 8 - P` against the inclusive
  interval `[-2^25 + 8, 2^25 + 4]` (nominal ±32 MiB). ARM-to-Thumb
  `PC24`/`PLT32`/`JUMP24` also need an in-range interworking veneer;
  `CALL` can rewrite to BLX in range.
- Thumb veneers check `D = target + A + 4 - P` for signed 23-bit reach
  (`[-2^22, 2^22)`, ±4 MiB), or signed 25-bit J1J2 reach
  (`[-2^24, 2^24)`, ±16 MiB). `THM_JUMP24` to ARM always needs a
  veneer; `THM_CALL` can rewrite to BLX in range. M-profile targets
  disable Thumb-to-ARM veneers. Short/conditional Thumb branches have no
  range-extension veneer. See [Veneers](#veneers-branch-island-stubs).
- `TARGET1` is scanned as `ABS32`; `TARGET2` selects `ABS32`, `REL32`,
  or `GOT_PREL` according to the target2 option. `GOTOFF32` itself does
  not reserve an individual target-symbol GOT entry.

Sources: [scanner](../../lib/Target/ARM/ARMRelocator.cpp),
[ARM-to-ARM](../../lib/Target/ARM/ARMToARMStub.cpp),
[ARM-to-Thumb](../../lib/Target/ARM/ARMToTHMStub.cpp),
[Thumb-to-ARM](../../lib/Target/ARM/THMToARMStub.cpp),
[Thumb-to-Thumb](../../lib/Target/ARM/THMToTHMStub.cpp).

## Relocation Conventions

| Symbol | Meaning |
|--------|---------|
| `S` | Runtime address of the referenced symbol |
| `A` | Relocation addend |
| `P` | Address of the relocation site (place) |
| `Pa` | `(P + 4) & ~3` — Thumb aligned PC (address of the place, PC-biased and aligned to 4 bytes) |
| `T` | 1 if the target symbol is a Thumb function, 0 otherwise |
| `U` | Add/subtract bit in the instruction encoding; 1 for a positive offset, 0 for a negative offset |
| `imm4H` | High 4 bits of an 8-bit immediate, encoded in instruction bits `[11:8]` |
| `imm4L` | Low 4 bits of an 8-bit immediate, encoded in instruction bits `[3:0]` |
| `B(S)` | Base address of the segment containing symbol `S` |
| `GOT_ORG` | Address of the Global Offset Table |
| `GOT(S)` | Address of the GOT entry for symbol `S` |
| `TLS` | TLS segment base address |
| `TP` | Thread pointer |

### The T Bit

ARM and Thumb functions are distinguished by the least-significant bit of a branch target. When `T = 1`, the expression `(S + A) \| T` sets bit 0, signalling an interworking branch to a Thumb function. Plain data relocations (`R_ARM_ABS16`, `R_ARM_ABS8`) do not OR in `T`.

---

## Absolute Relocations

| Relocation | Expression | Bits | Range Check | GOT allocation | PLT allocation | Trampoline / range |
|------------|-----------|------|-------------|-------------------------|-------------------------|--------------------|
| `R_ARM_ABS32` | `(S + A) \| T` | 32 | none | <ul><li><code>S: J[i]</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]; I: J[p&amp;f] + J[i]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]; I: L[p&amp;f] + R[i]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_ARM_ABS16` | `S + A` | 16 | none | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| `R_ARM_ABS8` | `S + A` | 8 | none | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| `R_ARM_MOVW_ABS_NC` | `(S + A) \| T` | `[15:0]` | none | <ul><li><code>S: J[i]</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]; I: J[p&amp;f] + J[i]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]; I: L[p&amp;f] + R[i]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_ARM_MOVT_ABS` | `S + A` | `[31:16]` | none | <ul><li><code>S: J[i]</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]; I: J[p&amp;f] + J[i]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]; I: L[p&amp;f] + R[i]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_ARM_THM_MOVW_ABS_NC` | `(S + A) \| T` | `[15:0]` | none | <ul><li><code>S: J[i]</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]; I: J[p&amp;f] + J[i]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]; I: L[p&amp;f] + R[i]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_ARM_THM_MOVT_ABS` | `S + A` | `[31:16]` | none | <ul><li><code>S: J[i]</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]; I: J[p&amp;f] + J[i]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]; I: L[p&amp;f] + R[i]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |

`R_ARM_MOVW_ABS_NC` and `R_ARM_MOVT_ABS` are used in pairs: `MOVW` loads bits `[15:0]` and `MOVT` loads bits `[31:16]` of a 32-bit address.

### Example: Loading a 32-bit Address (ARM)

```asm
movw r0, #:lower16:symbol   @ R_ARM_MOVW_ABS_NC
movt r0, #:upper16:symbol   @ R_ARM_MOVT_ABS
```

### Example: Loading a 32-bit Address (Thumb)

```asm
movw r0, #:lower16:symbol   @ R_ARM_THM_MOVW_ABS_NC
movt r0, #:upper16:symbol   @ R_ARM_THM_MOVT_ABS
```

---

## PC-Relative Relocations

| Relocation | Expression | Bits | Range Check | GOT allocation | PLT allocation | Trampoline / range |
|------------|-----------|------|-------------|-------------------------|-------------------------|--------------------|
| `R_ARM_REL32` | `((S + A) \| T) - P` | 32 | none | <ul><li><code>S: J[i]</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_SBREL32` | `((S + A) \| T) - P` | 32 | none | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_PREL31` | `((S + A) \| T) - P` | 31 | none | <ul><li><code>S: J[i]</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[i]</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[i]</code></li><li><code>H: L[p]</code></li></ul> | No |
| `R_ARM_BASE_PREL` | `B(S) + A - P` | 32 | none | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_MOVW_PREL_NC` | `((S + A) \| T) - P` | `[15:0]` | none | <ul><li><code>S: J[i]</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_MOVT_PREL` | `S + A - P` | `[31:16]` | none | <ul><li><code>S: J[i]</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_THM_MOVW_PREL_NC` | `((S + A) \| T) - P` | `[15:0]` | none | <ul><li><code>S: J[i]</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_THM_MOVT_PREL` | `S + A - P` | `[31:16]` | none | <ul><li><code>S: J[i]</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_THM_MOVT_BREL` | `S + A - P` | `[31:16]` | none | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_THM_MOVW_BREL_NC` | `((S + A) \| T) - B(S)` | `[15:0]` | none | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_THM_MOVW_BREL` | `((S + A) \| T) - B(S)` | `[15:0]` | none | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_ALU_PC_G0` | `((S + A) \| T) - P` | top 8 bits, 4-bit rotation | overflow checked | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_ALU_PC_G0_NC` | `((S + A) \| T) - P` | top 8 bits, 4-bit rotation | none (truncates) | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_ALU_PC_G1` | `((S + A) \| T) - P` | top 8 bits, 4-bit rotation | overflow checked | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_ALU_PC_G1_NC` | `((S + A) \| T) - P` | top 8 bits, 4-bit rotation | none (truncates) | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_ALU_PC_G2` | `((S + A) \| T) - P` | top 8 bits, 4-bit rotation | overflow checked | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_LDR_PC_G0` | `S + A - P` | imm12 (bits 11:0) | [0, 4095] | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_LDR_PC_G1` | `S + A - P` | imm12 (bits 11:0) | [0, 4095] | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_LDR_PC_G2` | `S + A - P` | imm12 (bits 11:0) | [0, 4095] | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_LDRS_PC_G0` | `S + A - P` | U bit + imm4H:imm4L | [0, 255] | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_LDRS_PC_G1` | `S + A - P` | U bit + imm4H:imm4L | [0, 255] | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_LDRS_PC_G2` | `S + A - P` | U bit + imm4H:imm4L | [0, 255] | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_LDC_PC_G0` | `S + A - P` | U bit + imm8, scaled by 4 | [0, 1023], 4-byte aligned | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_LDC_PC_G1` | `S + A - P` | U bit + imm8, scaled by 4 | [0, 1023], 4-byte aligned | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_LDC_PC_G2` | `S + A - P` | U bit + imm8, scaled by 4 | [0, 1023], 4-byte aligned | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |

`R_ARM_LDRS_PC_G0`, `R_ARM_LDRS_PC_G1`, and `R_ARM_LDRS_PC_G2` are memory-form group relocations.
They compute `X = S + A - P` and encode the residual selected by the
corresponding LDRS group mask. `U` encodes the sign and `imm4H:imm4L`
encodes the resulting 8-bit offset.
`R_ARM_LDC_PC_G0`, `R_ARM_LDC_PC_G1`, and `R_ARM_LDC_PC_G2` are LDC/STC
PC-relative group relocations. They compute `X = S + A - P` and encode the
residual selected by the corresponding LDC group mask. `U` encodes the sign
and `imm8` encodes the residual divided by four, requiring the residual to
be 4-byte aligned and no greater than 1023.

| Relocation | Expression | Encoding | Range | GOT allocation | PLT allocation | Trampoline / range |
|------------|------------|----------|-------|-------------------------|-------------------------|--------------------|
| `R_ARM_THM_PC8` | `S + A - Pa` | imm8, scaled by 4 | [0, 1023], 4-byte aligned | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_THM_PC12` | `((S + A) \| T) - Pa` | U bit + imm12 | [-4095, 4095] | <ul><li><code>S: J[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]{w}</code></li><li><code>H: -</code></li></ul> | No |

`R_ARM_THM_PC8` uses `Pa = (P + 4) & ~3` — the Thumb instruction address aligned to the next 4-byte boundary — instead of `P`.

`R_ARM_THM_PC12` uses `Pa = P & ~3`. The sign is encoded with the U bit and the magnitude in imm12.

`R_ARM_SBREL32` uses the same handler as `R_ARM_REL32` but produces a segment-base-relative offset. `R_ARM_PREL31` is used in ARM exception table entries.

---

## Branch Relocations

ARM branch instructions encode a signed byte offset divided by 4 into a 24-bit field, giving a range of ±32 MB. Thumb branch instructions encode the offset divided by 2.

### ARM Branch

| Relocation | Expression | Instruction | Encoded bits | Range | GOT allocation | PLT allocation | Trampoline / range |
|------------|-----------|-------------|--------------|-------|-------------------------|-------------------------|--------------------|
| `R_ARM_PC24` | `((S + A) \| T) - P` | `b` | 24-bit signed (`X >> 2`) | ±32 MB | <ul><li><code>S: J[i]</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[i]</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[i]</code></li><li><code>H: L[p]</code></li></ul> | ARM veneer; ±32 MiB or ISA switch |
| `R_ARM_PLT32` | `((S + A) \| T) - P` | `b` | 24-bit signed (`X >> 2`) | ±32 MB | <ul><li><code>S: J[i]</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[i]</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[i]</code></li><li><code>H: L[p]</code></li></ul> | ARM veneer; ±32 MiB or ISA switch |
| `R_ARM_JUMP24` | `((S + A) \| T) - P` | `b`/`bx` | 24-bit signed (`X >> 2`) | ±32 MB | <ul><li><code>S: J[i]</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[i]</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[i]</code></li><li><code>H: L[p]</code></li></ul> | ARM veneer; ±32 MiB or ISA switch |
| `R_ARM_CALL` | `((S + A) \| T) - P` | `bl`/`blx` | 24-bit signed (`X >> 2`) | ±32 MB | <ul><li><code>S: J[i]</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[i]</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[i]</code></li><li><code>H: L[p]</code></li></ul> | ARM veneer; outside ±32 MiB |

### Thumb Branch

| Relocation | Expression | Instruction | Encoded bits | Range | GOT allocation | PLT allocation | Trampoline / range |
|------------|-----------|-------------|--------------|-------|-------------------------|-------------------------|--------------------|
| `R_ARM_THM_JUMP8` | `S + A - P` | `b` (cond) | 8-bit signed (`X >> 1`) | ±256 B | <ul><li><code>S: J[i]{w}</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[i]{w}</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[i]{w}</code></li><li><code>H: L[p]</code></li></ul> | No |
| `R_ARM_THM_JUMP11` | `S + A - P` | `b` (uncond) | 11-bit signed (`X >> 1`) | ±2 KB | <ul><li><code>S: J[i]{w}</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[i]{w}</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[i]{w}</code></li><li><code>H: L[p]</code></li></ul> | No |
| `R_ARM_THM_JUMP19` | `((S + A) \| T) - P` | `b.cond` (Thumb-2) | 21-bit signed (`X >> 1`) | ±1 MB | <ul><li><code>S: J[i]{w}</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[i]{w}</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[i]{w}</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[i]{w}</code></li><li><code>H: L[p]</code></li></ul> | No |
| `R_ARM_THM_CALL` | `((S + A) \| T) - P` | `bl`/`blx` (Thumb-2) | 25-bit signed (`X >> 1`) | ±16 MB | <ul><li><code>S: J[i]</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[i]</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[i]</code></li><li><code>H: L[p]</code></li></ul> | Thumb veneer; outside ±4/16 MiB |
| `R_ARM_THM_JUMP24` | `((S + A) \| T) - P` | `b` (Thumb-2) | 25-bit signed (`X >> 1`) | ±16 MB | <ul><li><code>S: J[i]</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[i]</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[i]</code></li><li><code>H: L[p]</code></li></ul> | Thumb veneer; ±4/16 MiB or ISA switch |

---

## GOT-Relative Relocations

| Relocation | Expression | Bits | Range Check | GOT allocation | PLT allocation | Trampoline / range |
|------------|-----------|------|-------------|-------------------------|-------------------------|--------------------|
| `R_ARM_GOTOFF32` | `((S + A) \| T) - GOT_ORG` | 32 | none | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_GOT_BREL` | `GOT(S) + A - GOT_ORG` | 12 | none | <ul><li><code>S: J[i]{a} + G[!i]</code></li><li><code>D: G</code></li><li><code>P: G; I: J[i]{a} + G[!i]</code></li><li><code>H: G</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_GOT_PREL` | `GOT(S) + A - P` | 32 | none | <ul><li><code>S: J[i]{a} + G[!i]</code></li><li><code>D: G</code></li><li><code>P: G; I: J[i]{a} + G[!i]</code></li><li><code>H: G</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[i]</code></li><li><code>H: -</code></li></ul> | No |

---

## TLS Relocations

| Relocation | Expression | Notes | GOT allocation | PLT allocation | Trampoline / range |
|------------|-----------|-------|-------------------------|-------------------------|--------------------|
| `R_ARM_TLS_GD32` | `GOT(S) + A - P` | General Dynamic — two-GOT-entry sequence | <ul><li><code>S: Q[t]</code></li><li><code>D: Q[t]</code></li><li><code>P: Q[t]</code></li><li><code>H: Q[t]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_TLS_LDM32` | `GOT(S) + A - P` | Local Dynamic — module GOT entry | <ul><li><code>S: N[t]</code></li><li><code>D: N[t]</code></li><li><code>P: N[t]</code></li><li><code>H: N[t]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_TLS_IE32` | `GOT(S) + A - P` | Initial Exec — GOT-indirect TP offset | <ul><li><code>S: O[t]</code></li><li><code>D: O[t]</code></li><li><code>P: O[t]</code></li><li><code>H: O[t]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_TLS_LDO32` | `S + A - TLS` | Local Dynamic offset within TLS block | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_ARM_TLS_LE32` | `S + A + 2*WordSize + ...` | Local Exec — TP-relative offset, static only | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |

---

## Unsupported Relocations

The table below lists every relocation that ELD's ARM backend maps to the `unsupport` handler. Applying any of these to a non-dynamic object causes a link error. Relocations not listed here are handled. The **ABI category** column records how the ARM ELF ABI classifies the entry, and the **TODO** column records cases where the linker could reasonably add support.

| Type | Relocation | ABI category | TODO | GOT allocation | PLT allocation | Trampoline / range |
|------|-----------|--------------|------|-------------------------|-------------------------|--------------------|
| 5 | `R_ARM_ABS16` | 16-bit absolute | Implement 16-bit absolute | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 6 | `R_ARM_ABS12` | 12-bit absolute (LDR/STR immediate) | Implement ABS12 | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 7 | `R_ARM_THM_ABS5` | Thumb 5-bit absolute (LDR/STR) | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 8 | `R_ARM_ABS8` | 8-bit absolute | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 12 | `R_ARM_BREL_ADJ` | Dynamic — adjustment for R_ARM_TLS_DESC | Dynamic only | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 13 | `R_ARM_TLS_DESC` | Dynamic TLS descriptor | Dynamic only | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 14 | `R_ARM_THM_SWI8` | Obsolete | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 15 | `R_ARM_XPC25` | Obsolete | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 16 | `R_ARM_THM_XPC22` | Obsolete | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 17 | `R_ARM_TLS_DTPMOD32` | Dynamic TLS module index | Dynamic only | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 18 | `R_ARM_TLS_DTPOFF32` | Dynamic TLS symbol offset | Dynamic only | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 19 | `R_ARM_TLS_TPOFF32` | Dynamic TLS thread-pointer offset | Dynamic only | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 20 | `R_ARM_COPY` | Dynamic copy reloc | Dynamic only | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 21 | `R_ARM_GLOB_DAT` | Dynamic GOT fill | Dynamic only | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 22 | `R_ARM_JUMP_SLOT` | Dynamic PLT fill | Dynamic only | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 23 | `R_ARM_RELATIVE` | Dynamic base-relative | Dynamic only | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 31 | `R_ARM_BASE_ABS` | Segment-base absolute | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 32 | `R_ARM_ALU_PCREL_7_0` | Deprecated (ELF B02 name for ALU_PC_G0_NC) | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 33 | `R_ARM_ALU_PCREL_15_8` | Deprecated (ELF B02 name for ALU_PC_G1_NC) | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 34 | `R_ARM_ALU_PCREL_23_15` | Deprecated (ELF B02 name for ALU_PC_G2) | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 35 | `R_ARM_LDR_SBREL_11_0_NC` | Deprecated section-base relative LDR | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 36 | `R_ARM_ALU_SBREL_19_12_NC` | Deprecated section-base relative ALU | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 37 | `R_ARM_ALU_SBREL_27_20_CK` | Deprecated section-base relative ALU | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 39 | `R_ARM_SBREL31` | Deprecated 31-bit section-relative | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 52 | `R_ARM_THM_JUMP6` | Thumb 6-bit branch (CBZ/CBNZ) | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 53 | `R_ARM_THM_ALU_PREL_11_0` | Thumb-2 ALU PC-relative 11:0 | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 55 | `R_ARM_ABS32_NOI` | 32-bit absolute, no interworking bit | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 56 | `R_ARM_REL32_NOI` | 32-bit PC-relative, no interworking bit | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 70 | `R_ARM_ALU_SB_G0_NC` | Group reloc — ALU section-base G0, no overflow | Implement ALU_SB_G group | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 71 | `R_ARM_ALU_SB_G0` | Group reloc — ALU section-base G0 | Implement ALU_SB_G group | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 72 | `R_ARM_ALU_SB_G1_NC` | Group reloc — ALU section-base G1, no overflow | Implement ALU_SB_G group | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 73 | `R_ARM_ALU_SB_G1` | Group reloc — ALU section-base G1 | Implement ALU_SB_G group | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 74 | `R_ARM_ALU_SB_G2` | Group reloc — ALU section-base G2 | Implement ALU_SB_G group | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 75 | `R_ARM_LDR_SB_G0` | Group reloc — LDR section-base G0 | Implement LDR_SB_G group | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 76 | `R_ARM_LDR_SB_G1` | Group reloc — LDR section-base G1 | Implement LDR_SB_G group | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 77 | `R_ARM_LDR_SB_G2` | Group reloc — LDR section-base G2 | Implement LDR_SB_G group | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 78 | `R_ARM_LDRS_SB_G0` | Group reloc — LDRD/STRD section-base G0 | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 79 | `R_ARM_LDRS_SB_G1` | Group reloc — LDRD/STRD section-base G1 | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 80 | `R_ARM_LDRS_SB_G2` | Group reloc — LDRD/STRD section-base G2 | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 81 | `R_ARM_LDC_SB_G0` | Group reloc — LDC/STC section-base G0 | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 82 | `R_ARM_LDC_SB_G1` | Group reloc — LDC/STC section-base G1 | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 83 | `R_ARM_LDC_SB_G2` | Group reloc — LDC/STC section-base G2 | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 84 | `R_ARM_MOVW_BREL_NC` | MOVW section-base relative, no overflow | Implement MOVW/T_BREL (ARM) | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 85 | `R_ARM_MOVT_BREL` | MOVT section-base relative | Implement MOVW/T_BREL (ARM) | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 86 | `R_ARM_MOVW_BREL` | MOVW section-base relative | Implement MOVW/T_BREL (ARM) | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 90 | `R_ARM_TLS_GOTDESC` | TLS descriptor GOT entry | TLS-desc relaxation not implemented | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 91 | `R_ARM_TLS_CALL` | TLS descriptor call (ARM) | TLS-desc relaxation not implemented | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 92 | `R_ARM_TLS_DESCSEQ` | TLS descriptor sequence (ARM) | TLS-desc relaxation not implemented | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 93 | `R_ARM_THM_TLS_CALL` | TLS descriptor call (Thumb) | TLS-desc relaxation not implemented | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 94 | `R_ARM_PLT32_ABS` | PLT entry absolute address | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 95 | `R_ARM_GOT_ABS` | GOT entry absolute address | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 97 | `R_ARM_GOT_PREL12` | 12-bit GOT-relative (LDR/STR) | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 98 | `R_ARM_GOTOFF12` | 12-bit GOT-offset | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 99 | `R_ARM_GOTRELAX` | Linker relaxation hint | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 100 | `R_ARM_GNU_VTENTRY` | Deprecated GNU C++ vtable entry | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 101 | `R_ARM_GNU_VTINHERIT` | Deprecated GNU C++ vtable inheritance | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 109 | `R_ARM_TLS_LDO12` | TLS LDO 12-bit offset (LDR/STR) | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 110 | `R_ARM_TLS_LE12` | TLS LE 12-bit TP offset | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 111 | `R_ARM_TLS_IE12GP` | TLS IE 12-bit GOT-relative | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 112–127 | `R_ARM_PRIVATE_0–15` | Reserved for private use — not allocatable in portable objects | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 128 | `R_ARM_ME_TOO` | Obsolete | | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 129 | `R_ARM_THM_TLS_DESCSEQ16` | Thumb-1 TLS descriptor sequence | TLS-desc relaxation not implemented | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |
| 130 | `R_ARM_THM_TLS_DESCSEQ32` | Thumb-2 TLS descriptor sequence | TLS-desc relaxation not implemented | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: X</code></li><li><code>D: X</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | N/A |

---

## ABI Comparison Notes

The following discrepancies or gaps were found when comparing this document against the AAELF32 ABI specification:

| Area | Observed difference | TODO |
|------|--------------------|----- |
| `R_ARM_TLS_LE32` expression | Document shows `S + A + 2*WordSize + ...`; ABI defines it as `S + A - tp` (offset from thread pointer) | Fix expression in TLS table to `S + A - tp` |
| `R_ARM_GOT_BREL` bits column | Document shows `12`; ABI says the relocation places a 32-bit GOT-relative offset — the `12` refers to LDR's 12-bit immediate encoding, which is an instruction constraint, not the relocation size | Clarify as `32` with a note that the offset must fit in 12 bits |
| `R_ARM_SBREL32` | Documented as identical to `R_ARM_REL32`; ABI marks it as using the segment base `B(S)` rather than `P`, so the formula is `((S+A)\|T) - B(S)` not `-P` | Fix expression to `((S + A) \| T) - B(S)` |
| Thumb branch ranges | `R_ARM_THM_CALL` and `R_ARM_THM_JUMP24` are shown with `±16 MB` range; ABI specifies the range depends on whether J1J2 encoding is available (±4 MB without it) | Add conditional range note matching the Thumb Branch table in the Veneers section |
| `R_ARM_TARGET2` | Handled by ELD (as `target2`) but not documented in any section of this file | Add `R_ARM_TARGET2` to the PC-relative or absolute table with a note that its behaviour (ABS32 or GOT-relative) is controlled by a linker option |
| Deprecated relocations | `R_ARM_PC24` and `R_ARM_PLT32` are listed in the Branch section without noting they are deprecated in favour of `R_ARM_CALL`/`R_ARM_JUMP24` | Add deprecation note |

---

## Veneers (Branch Island Stubs)

When a branch target is out of range or requires an ISA mode switch (ARM ↔ Thumb), the linker inserts a small stub function called a **veneer** (also called a trampoline or branch island). The veneer is placed near the caller and jumps to the real target using an absolute or PC-relative address.

### When the Linker Creates Veneers

| Caller ISA | Target ISA | Relocation | Condition | Notes | GOT allocation | PLT allocation | Trampoline / range |
|------------|------------|------------|-----------|-------|-------------------------|-------------------------|--------------------|
| ARM | ARM | `R_ARM_PC24`, `R_ARM_CALL`, `R_ARM_JUMP24`, `R_ARM_PLT32` | Target out of ±32 MB range | No stub if target is Thumb (T bit = 1) | <ul><li><code>S: J[i]</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[i]</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[i]</code></li><li><code>H: L[p]</code></li></ul> | ARM veneer; ±32 MiB or ISA switch |
| ARM | Thumb | `R_ARM_PC24`, `R_ARM_JUMP24`, `R_ARM_PLT32` | Always — instruction cannot be rewritten as `blx` | `b`/`bl`/`b.w` cannot encode ISA switch directly | <ul><li><code>S: J[i]</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[i]</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[i]</code></li><li><code>H: L[p]</code></li></ul> | ARM veneer; ±32 MiB or ISA switch |
| ARM | Thumb | `R_ARM_CALL` | Target out of ±32 MB range | `bl` → `blx` rewrite is possible in range; stub used only if out of range | <ul><li><code>S: J[i]</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[i]</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[i]</code></li><li><code>H: L[p]</code></li></ul> | ARM veneer; outside ±32 MiB |
| Thumb | ARM | `R_ARM_THM_JUMP24` | Always — Thumb `b.w` cannot switch ISA | Disabled on microcontroller profiles (Cortex-M0) | <ul><li><code>S: J[i]</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[i]</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[i]</code></li><li><code>H: L[p]</code></li></ul> | Thumb veneer; ±4/16 MiB or ISA switch |
| Thumb | ARM | `R_ARM_THM_CALL` | Target out of ±4 MB range (23-bit) or ±16 MB range (25-bit J1J2) | Disabled on microcontroller profiles | <ul><li><code>S: J[i]</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[i]</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[i]</code></li><li><code>H: L[p]</code></li></ul> | Thumb veneer; outside ±4/16 MiB |
| Thumb | Thumb | `R_ARM_THM_JUMP24`, `R_ARM_THM_CALL` | Target out of ±4 MB range (23-bit) or ±16 MB range (25-bit J1J2) | No stub if target is ARM (T bit = 0) | <ul><li><code>S: J[i]</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[i]</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[i]</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[i]</code></li><li><code>H: L[p]</code></li></ul> | Thumb veneer; ±4/16 MiB or ISA switch |

> **Microcontroller profile:** For CPUs such as Cortex-M0 (and those with the M-profile CPU attribute), Thumb→ARM veneers are never created — the ARM ISA is not available. Thumb→Thumb veneers still use Thumb-1-safe templates (see below).
>
> **J1J2 encoding:** Thumb-2 processors with J1J2 branch encoding support a 25-bit offset (±16 MB); older Thumb-2 without J1J2 use a 23-bit offset (±4 MB). The linker detects this from the object attribute section.

### Branch Range Reference

| Caller ISA | Branch encoding | Range |
|------------|----------------|-------|
| ARM | 24-bit signed `× 4`, bias 8 | ±32 MB |
| Thumb | 23-bit signed `× 2`, bias 4 | ±4 MB |
| Thumb (J1J2) | 25-bit signed `× 2`, bias 4 | ±16 MB |

### Veneer Templates

The linker selects a veneer template based on link-time options and the target CPU profile:

| Template | Selected when | Instruction sequence |
|----------|--------------|----------------------|
| ABS (default) | Static link, no `-fPIC` | `ldr ip, [pc, #0]; bx ip; dcd target` |
| PIC | Position-independent code (`-fPIC` / `-fPIE`) or `--pic-veneer` | `ldr ip, [pc, #4]; add ip, pc, ip; bx ip; dcd offset` |
| MOV | `--use-mov-veneer` or microcontroller with MOVT/MOVW support | `movw ip, #lo16; movt ip, #hi16; bx ip` |
| THUMB1 | Microcontroller without MOVT/MOVW | `push {r0,r1}; ldr r0, [pc, #4]; str r0, [sp,#4]; pop {r0,pc}; dcd target` |

For Thumb→Thumb veneers the stub body first switches to ARM mode (`bx pc; nop`) before executing the ABS or PIC template, because the ARM load/branch sequence is simpler. The MOV and THUMB1 templates operate entirely in Thumb-2 or Thumb-1 respectively and therefore do not need that prefix.

Because the ABS/PIC Thumb→Thumb stub body switches ISA partway through, the fragment must carry correct ARM-ELF mapping symbols: `$t` at offset 0 (the `bx pc; nop` preamble, still Thumb) and `$a` at offset 4 (where the ARM-mode `ldr`/`add`/`bx` sequence actually begins). Getting these swapped doesn't corrupt the emitted code (the CPU executes the real bytes regardless) but it does corrupt mode-aware disassembly (`objdump -d`, debuggers) for the stub, since those tools trust the mapping symbols rather than re-deriving the mode.

---

## References

- ELF for the Arm Architecture (AAELF32)
  - https://github.com/ARM-software/abi-aa/blob/main/aaelf32/aaelf32.rst
