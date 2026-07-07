# AArch64 Relocation Prefixes

## Introduction

AArch64 relocation operators such as `:abs_g0_nc:`, `:pg_hi21:`, and `:lo12:` instruct the assembler and linker how to encode symbol references into instruction immediate fields.

## Relocation Conventions

| Symbol | Meaning |
|----------|---------|
| `S` | Runtime address of referenced symbol |
| `A` | Relocation addend |
| `P` | Address of relocation site |
| `X` | Relocation result before masking |
| `Page(expr)` | `expr & ~0xFFF` |
| `GOT` | Global Offset Table |

---

# Group Relocations

## Unsigned Absolute Relocations

| Operator | Relocation | Operation | Inst | Bits | Range Check |
|-----------|------------|------------|------|------|-------------|
| `:abs_g0:` | `MOVW_UABS_G0` | `S + A` | `movz` | `[15:0]` | `0 <= X < 2^16` |
| `:abs_g0_nc:` | `MOVW_UABS_G0_NC` | `S + A` | `movk` | `[15:0]` | none |
| `:abs_g1:` | `MOVW_UABS_G1` | `S + A` | `movz` | `[31:16]` | `0 <= X < 2^32` |
| `:abs_g1_nc:` | `MOVW_UABS_G1_NC` | `S + A` | `movk` | `[31:16]` | none |
| `:abs_g2:` | `MOVW_UABS_G2` | `S + A` | `movz` | `[47:32]` | `0 <= X < 2^48` |
| `:abs_g2_nc:` | `MOVW_UABS_G2_NC` | `S + A` | `movk` | `[47:32]` | none |
| `:abs_g3:` | `MOVW_UABS_G3` | `S + A` | `movz/movk` | `[63:48]` | none |

## Signed Absolute Relocations

| Operator | Relocation | Operation | Inst | Bits | Range Check |
|-----------|------------|------------|------|------|-------------|
| `:abs_g0_s:` | `MOVW_SABS_G0` | `S + A` | `movz/movn` | `[15:0]` | `-2^16 <= X < 2^16` |
| `:abs_g1_s:` | `MOVW_SABS_G1` | `S + A` | `movz/movn` | `[31:16]` | `-2^32 <= X < 2^32` |
| `:abs_g2_s:` | `MOVW_SABS_G2` | `S + A` | `movz/movn` | `[47:32]` | `-2^48 <= X < 2^48` |

## Example: Building a 64-bit Constant

```asm
movz x1, #:abs_g3:u64
movk x1, #:abs_g2_nc:u64
movk x1, #:abs_g1_nc:u64
movk x1, #:abs_g0_nc:u64
```

---

# PC-Relative Address Relocations

| Operator | Relocation | Expression | Instruction | Encoded Bits | Range Check |
|-----------|------------|------------|-------------|--------------|-------------|
| implicit | `LD_PREL_LO19` | `S+A-P` | literal `ldr` | `[20:2]` | `-2^20 <= X < 2^20` |
| implicit | `LD_PREL_LO21` | `S+A-P` | `adr` | `[20:0]` | `-2^20 <= X < 2^20` |
| `:pg_hi21:` | `ADR_PREL_PG_HI21` | `Page(S+A)-Page(P)` | `adrp` | `[31:12]` | `-2^32 <= X < 2^32` |
| `:pg_hi21_nc:` | `ADR_PREL_PG_HI21_NC` | `Page(S+A)-Page(P)` | `adrp` | `[31:12]` | none |

## Typical ADRP + ADD Sequence

```asm
adrp x0, :pg_hi21:foo
add  x0, x0, #:lo12:foo
```

---

# Low 12-bit Relocations

| Operator | Relocation | Used By | Immediate Extracted |
|-----------|------------|----------|--------------------|
| `:lo12:` | `ADD_ABS_LO12_NC` | `add` | `X[11:0]` |
| `:lo12:` | `LDST8_ABS_LO12_NC` | byte loads/stores | `X[11:0]` |
| `:lo12:` | `LDST16_ABS_LO12_NC` | halfword loads/stores | `X[11:1]` |
| `:lo12:` | `LDST32_ABS_LO12_NC` | word loads/stores | `X[11:2]` |
| `:lo12:` | `LDST64_ABS_LO12_NC` | doubleword loads/stores | `X[11:3]` |
| `:lo12:` | `LDST128_ABS_LO12_NC` | 128-bit accesses | `X[11:4]` |

---

# Control Flow Relocations

| Relocation | Instruction | Encoded Bits | Reach |
|------------|-------------|--------------|--------|
| `TSTBR14` | `tbz`, `tbnz` | `[15:2]` | `-2^15 <= X < 2^15` |
| `CONDBR19` | `b.<cond>` | `[20:2]` | `-2^20 <= X < 2^20` |
| `JUMP26` | `b` | `[27:2]` | `-2^27 <= X < 2^27` |
| `CALL26` | `bl` | `[27:2]` | `-2^27 <= X < 2^27` |

---

# Common Prefix Cheat Sheet

| Prefix | Meaning |
|----------|---------|
| `:abs_g0:` | Bits 15:0 |
| `:abs_g1:` | Bits 31:16 |
| `:abs_g2:` | Bits 47:32 |
| `:abs_g3:` | Bits 63:48 |
| `_nc` | No overflow check |
| `_s` | Signed relocation |
| `:pg_hi21:` | ADRP page-relative high 21 bits |
| `:lo12:` | Low 12 bits |


---

# Large Memory Model Relocations

The small and medium code models typically use ADRP+ADD sequences and are limited by the reach of ADRP. Large memory models use MOVZ/MOVK relocation sequences to construct arbitrary 64-bit addresses.

| Relocation | Operator | Instruction | Bits Loaded | Range Check |
|------------|------------|------------|------------|------------|
| `R_AARCH64_MOVW_UABS_G0` | `:abs_g0:` | `movz` | [15:0] | `0 <= X < 2^16` |
| `R_AARCH64_MOVW_UABS_G0_NC` | `:abs_g0_nc:` | `movk` | [15:0] | none |
| `R_AARCH64_MOVW_UABS_G1` | `:abs_g1:` | `movz` | [31:16] | `0 <= X < 2^32` |
| `R_AARCH64_MOVW_UABS_G1_NC` | `:abs_g1_nc:` | `movk` | [31:16] | none |
| `R_AARCH64_MOVW_UABS_G2` | `:abs_g2:` | `movz` | [47:32] | `0 <= X < 2^48` |
| `R_AARCH64_MOVW_UABS_G2_NC` | `:abs_g2_nc:` | `movk` | [47:32] | none |
| `R_AARCH64_MOVW_UABS_G3` | `:abs_g3:` | `movz/movk` | [63:48] | none |

## Example

```asm
movz x0, #:abs_g3:foo
movk x0, #:abs_g2_nc:foo
movk x0, #:abs_g1_nc:foo
movk x0, #:abs_g0_nc:foo
```

Resulting relocation sequence:

```text
R_AARCH64_MOVW_UABS_G3
R_AARCH64_MOVW_UABS_G2_NC
R_AARCH64_MOVW_UABS_G1_NC
R_AARCH64_MOVW_UABS_G0_NC
```

## GOT-Based Large Model Relocations

| Relocation | Purpose |
|------------|---------|
| `R_AARCH64_ADR_GOT_PAGE` | Compute GOT page address |
| `R_AARCH64_LD64_GOT_LO12_NC` | Load GOT entry offset |
| `R_AARCH64_GLOB_DAT` | Dynamic linker fills GOT entry |
| `R_AARCH64_JUMP_SLOT` | PLT/GOT function resolution |

## Reach Comparison

| Scheme | Reach |
|---------|---------|
| `ADR` | Approximately ±1 MB |
| `ADRP + ADD` | Approximately ±4 GB |
| `MOVZ/MOVK + MOVW_UABS_G*` | Full 64-bit address space |
| GOT-based sequence | Full 64-bit address space |


---

# AArch64 Code Models

## Small Code Model

Assumptions:
- Code and static data are reachable using PC-relative addressing.
- Symbols are typically accessed using `ADRP + ADD` or `ADRP + LDR/STR`.

Common relocations:

| Relocation | Purpose |
|------------|---------|
| `R_AARCH64_ADR_PREL_PG_HI21` | ADRP page calculation |
| `R_AARCH64_ADD_ABS_LO12_NC` | ADD low 12 bits |
| `R_AARCH64_LDST8_ABS_LO12_NC` | 8-bit load/store offset |
| `R_AARCH64_LDST16_ABS_LO12_NC` | 16-bit load/store offset |
| `R_AARCH64_LDST32_ABS_LO12_NC` | 32-bit load/store offset |
| `R_AARCH64_LDST64_ABS_LO12_NC` | 64-bit load/store offset |

Example:

```asm
adrp x0, :pg_hi21:global
add  x0, x0, #:lo12:global
```

## Medium Code Model

Assumptions:
- Code remains reachable through branch relocations.
- Data may reside outside the ADRP reach window.
- Large objects are frequently accessed through the GOT.

Common relocations:

| Relocation | Purpose |
|------------|---------|
| `R_AARCH64_ADR_GOT_PAGE` | Compute GOT page |
| `R_AARCH64_LD64_GOT_LO12_NC` | Access GOT entry |
| `R_AARCH64_GLOB_DAT` | Dynamic linker fills GOT |
| `R_AARCH64_JUMP_SLOT` | Function resolution via PLT/GOT |

Example:

```asm
adrp x0, :got:symbol
ldr  x0, [x0, #:got_lo12:symbol]
```

## Large Code Model

Assumptions:
- Code and data may be located anywhere in the 64-bit address space.
- Full-width addresses are materialized using MOVZ/MOVK relocation sequences.

Common relocations:

| Relocation |
|------------|
| `R_AARCH64_MOVW_UABS_G0` |
| `R_AARCH64_MOVW_UABS_G0_NC` |
| `R_AARCH64_MOVW_UABS_G1` |
| `R_AARCH64_MOVW_UABS_G1_NC` |
| `R_AARCH64_MOVW_UABS_G2` |
| `R_AARCH64_MOVW_UABS_G2_NC` |
| `R_AARCH64_MOVW_UABS_G3` |

Example:

```asm
movz x0, #:abs_g3:symbol
movk x0, #:abs_g2_nc:symbol
movk x0, #:abs_g1_nc:symbol
movk x0, #:abs_g0_nc:symbol
```


---

# References

## Official AArch64 ELF ABI Specification

### Current Source (GitHub)

- ELF for the Arm® 64-bit Architecture (AArch64)
  - https://github.com/ARM-software/abi-aa/blob/main/aaelf64/aaelf64.rst

### AAELF64 Repository Directory

- https://github.com/ARM-software/abi-aa/tree/main/aaelf64

## Recommended Sections

When implementing linker relocations, the most useful sections are:

- Relocation Processing
- Static AArch64 Relocations
- Dynamic Relocations
- GOT and PLT Relocations
- TLS Relocations
- Code Models
- Program Loading and Dynamic Linking
