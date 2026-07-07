# ARM Relocation Reference

## Introduction

ARM (32-bit) supports two instruction set architectures: the 32-bit ARM ISA and the 16/32-bit Thumb/Thumb-2 ISA. Relocations encode symbol references into instruction immediate fields and, for branch relocations, into the encoded offset. The linker selects ARM or Thumb encoding based on whether the target symbol is a Thumb function (the T bit).

## Relocation Conventions

| Symbol | Meaning |
|--------|---------|
| `S` | Runtime address of the referenced symbol |
| `A` | Relocation addend |
| `P` | Address of the relocation site (place) |
| `T` | 1 if the target symbol is a Thumb function, 0 otherwise |
| `B(S)` | Base address of the segment containing symbol `S` |
| `GOT_ORG` | Address of the Global Offset Table |
| `GOT(S)` | Address of the GOT entry for symbol `S` |
| `TLS` | TLS segment base address |
| `TP` | Thread pointer |

### The T Bit

ARM and Thumb functions are distinguished by the least-significant bit of a branch target. When `T = 1`, the expression `(S + A) \| T` sets bit 0, signalling an interworking branch to a Thumb function. Plain data relocations (`R_ARM_ABS16`, `R_ARM_ABS8`) do not OR in `T`.

---

# Absolute Relocations

| Relocation | Expression | Bits | Range Check |
|------------|-----------|------|-------------|
| `R_ARM_ABS32` | `(S + A) \| T` | 32 | none |
| `R_ARM_ABS16` | `S + A` | 16 | none |
| `R_ARM_ABS8` | `S + A` | 8 | none |
| `R_ARM_MOVW_ABS_NC` | `(S + A) \| T` | `[15:0]` | none |
| `R_ARM_MOVT_ABS` | `S + A` | `[31:16]` | none |
| `R_ARM_THM_MOVW_ABS_NC` | `(S + A) \| T` | `[15:0]` | none |
| `R_ARM_THM_MOVT_ABS` | `S + A` | `[31:16]` | none |

`R_ARM_MOVW_ABS_NC` and `R_ARM_MOVT_ABS` are used in pairs: `MOVW` loads bits `[15:0]` and `MOVT` loads bits `[31:16]` of a 32-bit address.

## Example: Loading a 32-bit Address (ARM)

```asm
movw r0, #:lower16:symbol   @ R_ARM_MOVW_ABS_NC
movt r0, #:upper16:symbol   @ R_ARM_MOVT_ABS
```

## Example: Loading a 32-bit Address (Thumb)

```asm
movw r0, #:lower16:symbol   @ R_ARM_THM_MOVW_ABS_NC
movt r0, #:upper16:symbol   @ R_ARM_THM_MOVT_ABS
```

---

# PC-Relative Relocations

| Relocation | Expression | Bits | Range Check |
|------------|-----------|------|-------------|
| `R_ARM_REL32` | `((S + A) \| T) - P` | 32 | none |
| `R_ARM_SBREL32` | `((S + A) \| T) - P` | 32 | none |
| `R_ARM_PREL31` | `((S + A) \| T) - P` | 31 | none |
| `R_ARM_BASE_PREL` | `B(S) + A - P` | 32 | none |
| `R_ARM_MOVW_PREL_NC` | `((S + A) \| T) - P` | `[15:0]` | none |
| `R_ARM_MOVT_PREL` | `S + A - P` | `[31:16]` | none |
| `R_ARM_THM_MOVW_PREL_NC` | `((S + A) \| T) - P` | `[15:0]` | none |
| `R_ARM_THM_MOVT_PREL` | `S + A - P` | `[31:16]` | none |
| `R_ARM_THM_MOVT_BREL` | `S + A - P` | `[31:16]` | none |
| `R_ARM_THM_MOVW_BREL_NC` | `((S + A) \| T) - B(S)` | `[15:0]` | none |
| `R_ARM_THM_MOVW_BREL` | `((S + A) \| T) - B(S)` | `[15:0]` | none |
| `R_ARM_ALU_PC_G0` | `((S + A) \| T) - P` | top 8 bits, 4-bit rotation | none |

`R_ARM_SBREL32` uses the same handler as `R_ARM_REL32` but produces a segment-base-relative offset. `R_ARM_PREL31` is used in ARM exception table entries.

---

# Branch Relocations

ARM branch instructions encode a signed byte offset divided by 4 into a 24-bit field, giving a range of ±32 MB. Thumb branch instructions encode the offset divided by 2.

## ARM Branch

| Relocation | Expression | Instruction | Encoded bits | Range |
|------------|-----------|-------------|--------------|-------|
| `R_ARM_PC24` | `((S + A) \| T) - P` | `b` | 24-bit signed (`X >> 2`) | ±32 MB |
| `R_ARM_PLT32` | `((S + A) \| T) - P` | `b` | 24-bit signed (`X >> 2`) | ±32 MB |
| `R_ARM_JUMP24` | `((S + A) \| T) - P` | `b`/`bx` | 24-bit signed (`X >> 2`) | ±32 MB |
| `R_ARM_CALL` | `((S + A) \| T) - P` | `bl`/`blx` | 24-bit signed (`X >> 2`) | ±32 MB |

## Thumb Branch

| Relocation | Expression | Instruction | Encoded bits | Range |
|------------|-----------|-------------|--------------|-------|
| `R_ARM_THM_JUMP8` | `S + A - P` | `b` (cond) | 8-bit signed (`X >> 1`) | ±256 B |
| `R_ARM_THM_JUMP11` | `S + A - P` | `b` (uncond) | 11-bit signed (`X >> 1`) | ±2 KB |
| `R_ARM_THM_JUMP19` | `((S + A) \| T) - P` | `b.cond` (Thumb-2) | 21-bit signed (`X >> 1`) | ±1 MB |
| `R_ARM_THM_CALL` | `((S + A) \| T) - P` | `bl`/`blx` (Thumb-2) | 25-bit signed (`X >> 1`) | ±16 MB |
| `R_ARM_THM_JUMP24` | `((S + A) \| T) - P` | `b` (Thumb-2) | 25-bit signed (`X >> 1`) | ±16 MB |

---

# GOT-Relative Relocations

| Relocation | Expression | Bits | Range Check |
|------------|-----------|------|-------------|
| `R_ARM_GOTOFF32` | `((S + A) \| T) - GOT_ORG` | 32 | none |
| `R_ARM_GOT_BREL` | `GOT(S) + A - GOT_ORG` | 12 | none |
| `R_ARM_GOT_PREL` | `GOT(S) + A - P` | 32 | none |

---

# TLS Relocations

| Relocation | Expression | Notes |
|------------|-----------|-------|
| `R_ARM_TLS_GD32` | `GOT(S) + A - P` | General Dynamic — two-GOT-entry sequence |
| `R_ARM_TLS_LDM32` | `GOT(S) + A - P` | Local Dynamic — module GOT entry |
| `R_ARM_TLS_IE32` | `GOT(S) + A - P` | Initial Exec — GOT-indirect TP offset |
| `R_ARM_TLS_LDO32` | `S + A - TLS` | Local Dynamic offset within TLS block |
| `R_ARM_TLS_LE32` | `S + A + 2*WordSize + ...` | Local Exec — TP-relative offset, static only |

---

# Unsupported Relocations

The table below lists every relocation that ELD's ARM backend maps to the `unsupport` handler. Applying any of these to a non-dynamic object causes a link error. Relocations not listed here are handled. The **ABI category** column records how the ARM ELF ABI classifies the entry, and the **TODO** column records cases where the linker could reasonably add support.

| Type | Relocation | ABI category | TODO |
|------|-----------|--------------|------|
| 4 | `R_ARM_LDR_PC_G0` | Group reloc — literal-pool PC-relative LDR (G0) | Implement LDR PC-group G0 |
| 5 | `R_ARM_ABS16` | 16-bit absolute | Implement 16-bit absolute |
| 6 | `R_ARM_ABS12` | 12-bit absolute (LDR/STR immediate) | Implement ABS12 |
| 7 | `R_ARM_THM_ABS5` | Thumb 5-bit absolute (LDR/STR) | |
| 8 | `R_ARM_ABS8` | 8-bit absolute | |
| 11 | `R_ARM_THM_PC8` | Thumb 8-bit PC-relative (LDR literal) | |
| 12 | `R_ARM_BREL_ADJ` | Dynamic — adjustment for R_ARM_TLS_DESC | Dynamic only |
| 13 | `R_ARM_TLS_DESC` | Dynamic TLS descriptor | Dynamic only |
| 14 | `R_ARM_THM_SWI8` | Obsolete | |
| 15 | `R_ARM_XPC25` | Obsolete | |
| 16 | `R_ARM_THM_XPC22` | Obsolete | |
| 17 | `R_ARM_TLS_DTPMOD32` | Dynamic TLS module index | Dynamic only |
| 18 | `R_ARM_TLS_DTPOFF32` | Dynamic TLS symbol offset | Dynamic only |
| 19 | `R_ARM_TLS_TPOFF32` | Dynamic TLS thread-pointer offset | Dynamic only |
| 20 | `R_ARM_COPY` | Dynamic copy reloc | Dynamic only |
| 21 | `R_ARM_GLOB_DAT` | Dynamic GOT fill | Dynamic only |
| 22 | `R_ARM_JUMP_SLOT` | Dynamic PLT fill | Dynamic only |
| 23 | `R_ARM_RELATIVE` | Dynamic base-relative | Dynamic only |
| 31 | `R_ARM_BASE_ABS` | Segment-base absolute | |
| 32 | `R_ARM_ALU_PCREL_7_0` | Deprecated (ELF B02 name for ALU_PC_G0_NC) | |
| 33 | `R_ARM_ALU_PCREL_15_8` | Deprecated (ELF B02 name for ALU_PC_G1_NC) | |
| 34 | `R_ARM_ALU_PCREL_23_15` | Deprecated (ELF B02 name for ALU_PC_G2) | |
| 35 | `R_ARM_LDR_SBREL_11_0_NC` | Deprecated section-base relative LDR | |
| 36 | `R_ARM_ALU_SBREL_19_12_NC` | Deprecated section-base relative ALU | |
| 37 | `R_ARM_ALU_SBREL_27_20_CK` | Deprecated section-base relative ALU | |
| 39 | `R_ARM_SBREL31` | Deprecated 31-bit section-relative | |
| 52 | `R_ARM_THM_JUMP6` | Thumb 6-bit branch (CBZ/CBNZ) | |
| 53 | `R_ARM_THM_ALU_PREL_11_0` | Thumb-2 ALU PC-relative 11:0 | |
| 54 | `R_ARM_THM_PC12` | Thumb-2 LDR/STR 12-bit PC-relative | |
| 55 | `R_ARM_ABS32_NOI` | 32-bit absolute, no interworking bit | |
| 56 | `R_ARM_REL32_NOI` | 32-bit PC-relative, no interworking bit | |
| 57 | `R_ARM_ALU_PC_G0_NC` | Group reloc — ALU PC-relative G0, no overflow | Implement ALU_PC_G group |
| 59 | `R_ARM_ALU_PC_G1_NC` | Group reloc — ALU PC-relative G1, no overflow | Implement ALU_PC_G group |
| 60 | `R_ARM_ALU_PC_G1` | Group reloc — ALU PC-relative G1 | Implement ALU_PC_G group |
| 61 | `R_ARM_ALU_PC_G2` | Group reloc — ALU PC-relative G2 | Implement ALU_PC_G group |
| 62 | `R_ARM_LDR_PC_G1` | Group reloc — LDR PC-relative G1 | Implement LDR PC-group |
| 63 | `R_ARM_LDR_PC_G2` | Group reloc — LDR PC-relative G2 | Implement LDR PC-group |
| 64 | `R_ARM_LDRS_PC_G0` | Group reloc — LDRD/STRD PC-relative G0 | Implement LDRS PC-group |
| 65 | `R_ARM_LDRS_PC_G1` | Group reloc — LDRD/STRD PC-relative G1 | Implement LDRS PC-group |
| 66 | `R_ARM_LDRS_PC_G2` | Group reloc — LDRD/STRD PC-relative G2 | Implement LDRS PC-group |
| 67 | `R_ARM_LDC_PC_G0` | Group reloc — LDC/STC PC-relative G0 | |
| 68 | `R_ARM_LDC_PC_G1` | Group reloc — LDC/STC PC-relative G1 | |
| 69 | `R_ARM_LDC_PC_G2` | Group reloc — LDC/STC PC-relative G2 | |
| 70 | `R_ARM_ALU_SB_G0_NC` | Group reloc — ALU section-base G0, no overflow | Implement ALU_SB_G group |
| 71 | `R_ARM_ALU_SB_G0` | Group reloc — ALU section-base G0 | Implement ALU_SB_G group |
| 72 | `R_ARM_ALU_SB_G1_NC` | Group reloc — ALU section-base G1, no overflow | Implement ALU_SB_G group |
| 73 | `R_ARM_ALU_SB_G1` | Group reloc — ALU section-base G1 | Implement ALU_SB_G group |
| 74 | `R_ARM_ALU_SB_G2` | Group reloc — ALU section-base G2 | Implement ALU_SB_G group |
| 75 | `R_ARM_LDR_SB_G0` | Group reloc — LDR section-base G0 | Implement LDR_SB_G group |
| 76 | `R_ARM_LDR_SB_G1` | Group reloc — LDR section-base G1 | Implement LDR_SB_G group |
| 77 | `R_ARM_LDR_SB_G2` | Group reloc — LDR section-base G2 | Implement LDR_SB_G group |
| 78 | `R_ARM_LDRS_SB_G0` | Group reloc — LDRD/STRD section-base G0 | |
| 79 | `R_ARM_LDRS_SB_G1` | Group reloc — LDRD/STRD section-base G1 | |
| 80 | `R_ARM_LDRS_SB_G2` | Group reloc — LDRD/STRD section-base G2 | |
| 81 | `R_ARM_LDC_SB_G0` | Group reloc — LDC/STC section-base G0 | |
| 82 | `R_ARM_LDC_SB_G1` | Group reloc — LDC/STC section-base G1 | |
| 83 | `R_ARM_LDC_SB_G2` | Group reloc — LDC/STC section-base G2 | |
| 84 | `R_ARM_MOVW_BREL_NC` | MOVW section-base relative, no overflow | Implement MOVW/T_BREL (ARM) |
| 85 | `R_ARM_MOVT_BREL` | MOVT section-base relative | Implement MOVW/T_BREL (ARM) |
| 86 | `R_ARM_MOVW_BREL` | MOVW section-base relative | Implement MOVW/T_BREL (ARM) |
| 90 | `R_ARM_TLS_GOTDESC` | TLS descriptor GOT entry | TLS-desc relaxation not implemented |
| 91 | `R_ARM_TLS_CALL` | TLS descriptor call (ARM) | TLS-desc relaxation not implemented |
| 92 | `R_ARM_TLS_DESCSEQ` | TLS descriptor sequence (ARM) | TLS-desc relaxation not implemented |
| 93 | `R_ARM_THM_TLS_CALL` | TLS descriptor call (Thumb) | TLS-desc relaxation not implemented |
| 94 | `R_ARM_PLT32_ABS` | PLT entry absolute address | |
| 95 | `R_ARM_GOT_ABS` | GOT entry absolute address | |
| 97 | `R_ARM_GOT_PREL12` | 12-bit GOT-relative (LDR/STR) | |
| 98 | `R_ARM_GOTOFF12` | 12-bit GOT-offset | |
| 99 | `R_ARM_GOTRELAX` | Linker relaxation hint | |
| 100 | `R_ARM_GNU_VTENTRY` | Deprecated GNU C++ vtable entry | |
| 101 | `R_ARM_GNU_VTINHERIT` | Deprecated GNU C++ vtable inheritance | |
| 109 | `R_ARM_TLS_LDO12` | TLS LDO 12-bit offset (LDR/STR) | |
| 110 | `R_ARM_TLS_LE12` | TLS LE 12-bit TP offset | |
| 111 | `R_ARM_TLS_IE12GP` | TLS IE 12-bit GOT-relative | |
| 112–127 | `R_ARM_PRIVATE_0–15` | Reserved for private use — not allocatable in portable objects | |
| 128 | `R_ARM_ME_TOO` | Obsolete | |
| 129 | `R_ARM_THM_TLS_DESCSEQ16` | Thumb-1 TLS descriptor sequence | TLS-desc relaxation not implemented |
| 130 | `R_ARM_THM_TLS_DESCSEQ32` | Thumb-2 TLS descriptor sequence | TLS-desc relaxation not implemented |

---

# ABI Comparison Notes

The following discrepancies or gaps were found when comparing this document against the AAELF32 ABI specification:

| Area | Observed difference | TODO |
|------|--------------------|----- |
| `R_ARM_TLS_LE32` expression | Document shows `S + A + 2*WordSize + ...`; ABI defines it as `S + A - tp` (offset from thread pointer) | Fix expression in TLS table to `S + A - tp` |
| `R_ARM_GOT_BREL` bits column | Document shows `12`; ABI says the relocation places a 32-bit GOT-relative offset — the `12` refers to LDR's 12-bit immediate encoding, which is an instruction constraint, not the relocation size | Clarify as `32` with a note that the offset must fit in 12 bits |
| `R_ARM_SBREL32` | Documented as identical to `R_ARM_REL32`; ABI marks it as using the segment base `B(S)` rather than `P`, so the formula is `((S+A)\|T) - B(S)` not `-P` | Fix expression to `((S + A) \| T) - B(S)` |
| `R_ARM_ALU_PC_G0` (type 58) | Documented in this file and handled by ELD via the `alu_pc` handler; the G1/G2 variants and the `_NC` forms (57, 59-61) are unsupported — not noted in the PC-relative table | Add a note that only G0 is supported |
| Thumb branch ranges | `R_ARM_THM_CALL` and `R_ARM_THM_JUMP24` are shown with `±16 MB` range; ABI specifies the range depends on whether J1J2 encoding is available (±4 MB without it) | Add conditional range note matching the Thumb Branch table in the Veneers section |
| `R_ARM_TARGET2` | Handled by ELD (as `target2`) but not documented in any section of this file | Add `R_ARM_TARGET2` to the PC-relative or absolute table with a note that its behaviour (ABS32 or GOT-relative) is controlled by a linker option |
| Deprecated relocations | `R_ARM_PC24` and `R_ARM_PLT32` are listed in the Branch section without noting they are deprecated in favour of `R_ARM_CALL`/`R_ARM_JUMP24` | Add deprecation note |

---

# Veneers (Branch Island Stubs)

When a branch target is out of range or requires an ISA mode switch (ARM ↔ Thumb), the linker inserts a small stub function called a **veneer** (also called a trampoline or branch island). The veneer is placed near the caller and jumps to the real target using an absolute or PC-relative address.

## When the Linker Creates Veneers

| Caller ISA | Target ISA | Relocation | Condition | Notes |
|------------|------------|------------|-----------|-------|
| ARM | ARM | `R_ARM_PC24`, `R_ARM_CALL`, `R_ARM_JUMP24`, `R_ARM_PLT32` | Target out of ±32 MB range | No stub if target is Thumb (T bit = 1) |
| ARM | Thumb | `R_ARM_PC24`, `R_ARM_JUMP24`, `R_ARM_PLT32` | Always — instruction cannot be rewritten as `blx` | `b`/`bl`/`b.w` cannot encode ISA switch directly |
| ARM | Thumb | `R_ARM_CALL` | Target out of ±32 MB range | `bl` → `blx` rewrite is possible in range; stub used only if out of range |
| Thumb | ARM | `R_ARM_THM_JUMP24` | Always — Thumb `b.w` cannot switch ISA | Disabled on microcontroller profiles (Cortex-M0) |
| Thumb | ARM | `R_ARM_THM_CALL` | Target out of ±4 MB range (23-bit) or ±16 MB range (25-bit J1J2) | Disabled on microcontroller profiles |
| Thumb | Thumb | `R_ARM_THM_JUMP24`, `R_ARM_THM_CALL` | Target out of ±4 MB range (23-bit) or ±16 MB range (25-bit J1J2) | No stub if target is ARM (T bit = 0) |

> **Microcontroller profile:** For CPUs such as Cortex-M0 (and those with the M-profile CPU attribute), Thumb→ARM veneers are never created — the ARM ISA is not available. Thumb→Thumb veneers still use Thumb-1-safe templates (see below).
>
> **J1J2 encoding:** Thumb-2 processors with J1J2 branch encoding support a 25-bit offset (±16 MB); older Thumb-2 without J1J2 use a 23-bit offset (±4 MB). The linker detects this from the object attribute section.

## Branch Range Reference

| Caller ISA | Branch encoding | Range |
|------------|----------------|-------|
| ARM | 24-bit signed `× 4`, bias 8 | ±32 MB |
| Thumb | 23-bit signed `× 2`, bias 4 | ±4 MB |
| Thumb (J1J2) | 25-bit signed `× 2`, bias 4 | ±16 MB |

## Veneer Templates

The linker selects a veneer template based on link-time options and the target CPU profile:

| Template | Selected when | Instruction sequence |
|----------|--------------|----------------------|
| ABS (default) | Static link, no `-fPIC` | `ldr ip, [pc, #0]; bx ip; dcd target` |
| PIC | Position-independent code (`-fPIC` / `-fPIE`) | `ldr ip, [pc, #4]; add ip, pc, ip; bx ip; dcd offset` |
| MOV | `--use-mov-veneer` or microcontroller with MOVT/MOVW support | `movw ip, #lo16; movt ip, #hi16; bx ip` |
| THUMB1 | Microcontroller without MOVT/MOVW | `push {r0,r1}; ldr r0, [pc, #4]; str r0, [sp,#4]; pop {r0,pc}; dcd target` |

For Thumb→Thumb veneers the stub body first switches to ARM mode (`bx pc; nop`) before executing the ABS or PIC template, because the ARM load/branch sequence is simpler. The MOV and THUMB1 templates operate entirely in Thumb-2 or Thumb-1 respectively and therefore do not need that prefix.

---

# References

- ELF for the Arm Architecture (AAELF32)
  - https://github.com/ARM-software/abi-aa/blob/main/aaelf32/aaelf32.rst
