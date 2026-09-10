# RISC-V Relocation Reference

## Introduction

This document describes the RISC-V relocations handled by ELD, including
their encoding types, alignment requirements, range checks, and the
relaxations that transform instruction sequences at link time.

The official ABI specification is the RISC-V ELF psABI:
https://github.com/riscv-non-isa/riscv-elf-psabi-doc/blob/master/riscv-elf.adoc

---

## Relocation Conventions

| Symbol | Meaning |
|--------|---------|
| `S` | Runtime address of the referenced symbol |
| `A` | Relocation addend |
| `P` | Address of the relocation site |
| `G` | Address of the global pointer (`__global_pointer$`) |
| `GOT` | Global Offset Table base address |
| `X` | Relocation result before masking/encoding |

---

## Encoding Types

Each relocation carries an encoding type that describes which instruction
immediate field is written and how the value is packed into it.  The
"Immediate bits written" column shows which bits of the computed relocation
value `X` are placed into which instruction word bits, matching the
`encode*` helpers in `RISCVHelper.h`.

### Unscrambled (linear) encodings

| Encoding       | Instruction format       | Value bits → instruction bits          | Width | Inst size |
|----------------|--------------------------|----------------------------------------|-------|-----------|
| `EncTy_None`   | No field written         | —                                      | —     | —         |
| `EncTy_8`      | Data byte                | `X[7:0]` → `inst[7:0]`                | 8     | 8-bit     |
| `EncTy_16`     | Data halfword            | `X[15:0]` → `inst[15:0]`              | 16    | 16-bit    |
| `EncTy_32`     | Data word                | `X[31:0]` → `inst[31:0]`              | 32    | 32-bit    |
| `EncTy_64`     | Data doubleword          | `X[63:0]` → `inst[63:0]`              | 64    | 64-bit    |
| `EncTy_6`      | Byte, low 6 bits         | `X[5:0]` → `inst[5:0]`                | 6     | 8-bit     |
| `EncTy_LEB128` | ULEB128 data             | Variable-length unsigned LEB128        | var   | variable  |

### 32-bit instruction encodings

| Encoding        | Instruction format           | Value bits → instruction bits                                                                 | Width |
|-----------------|------------------------------|-----------------------------------------------------------------------------------------------|-------|
| `EncTy_I`       | I-type (`addi`, `lw`, …)     | `X[11:0]` → `inst[31:20]`                                                                    | 12    |
| `EncTy_S`       | S-type (`sw`, `sb`, …)       | `X[11:5]` → `inst[31:25]`<br>`X[4:0]` → `inst[11:7]`                                        | 12    |
| `EncTy_SB`      | B-type (branch)              | `X[12]` → `inst[31]`<br>`X[10:5]` → `inst[30:25]`<br>`X[4:1]` → `inst[11:8]`<br>`X[11]` → `inst[7]` | 13 (signed, 2-byte aligned) |
| `EncTy_UJ`      | J-type (`jal`)               | `X[20]` → `inst[31]`<br>`X[10:1]` → `inst[30:21]`<br>`X[11]` → `inst[20]`<br>`X[19:12]` → `inst[19:12]` | 21 (signed, 2-byte aligned) |
| `EncTy_U_HI20`  | U-type (`lui`, `auipc`)      | `(X + 0x800)[31:12]` → `inst[31:12]`  (carry-adjusted upper 20 bits)                        | 20    |
| `EncTy_U_ABS20` | Xqci 20-bit U-type           | `X[19]` → `inst[31]`<br>`X[14:0]` → `inst[30:16]`<br>`X[18:15]` → `inst[15:12]`            | 20    |

> **`EncTy_U_HI20` carry adjustment:** `0x800` is added before masking so that
> when the low 12-bit `addi` immediate is sign-extended, it correctly
> reconstructs the original address together with the upper 20 bits.

### 16-bit (RVC) instruction encodings

| Encoding  | Instruction format         | Value bits → instruction bits                                                                                                       | Width |
|-----------|----------------------------|-------------------------------------------------------------------------------------------------------------------------------------|-------|
| `EncTy_CB` | C.B-type (RVC branch)     | `X[8]` → `inst[12]`<br>`X[4:3]` → `inst[11:10]`<br>`X[7:6]` → `inst[6:5]`<br>`X[2:1]` → `inst[4:3]`<br>`X[5]` → `inst[2]`      | 9 (signed, 2-byte aligned) |
| `EncTy_CJ` | C.J-type (RVC jump)       | `X[11]` → `inst[12]`<br>`X[4]` → `inst[11]`<br>`X[9:8]` → `inst[10:9]`<br>`X[10]` → `inst[8]`<br>`X[6]` → `inst[7]`<br>`X[7]` → `inst[6]`<br>`X[3:1]` → `inst[5:3]`<br>`X[5]` → `inst[2]` | 12 (signed, 2-byte aligned) |
| `EncTy_CI` | C.I-type (`c.lui`, `c.li`) | `X[5+shift]` → `inst[12]`<br>`X[4+shift:shift]` → `inst[6:2]`<br>(shift=12 for `c.lui`, shift=0 for `c.li`) | 6     |

### 48-bit (Xqci/Xqcilo) instruction encodings

| Encoding      | Instruction format              | Value bits → instruction bits                                                                                                                          | Width |
|---------------|---------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------|-------|
| `EncTy_QC_EI` | Xqcilo E-I-type load            | `X[9:0]` → `inst[29:20]`<br>`X[25:10]` → `inst[47:32]`                                                                                              | 26 (signed) |
| `EncTy_QC_ES` | Xqcilo E-S-type store           | `X[4:0]` → `inst[11:7]`<br>`X[9:5]` → `inst[31:25]`<br>`X[25:10]` → `inst[47:32]`                                                                  | 26 (signed) |
| `EncTy_QC_EB` | Xqci E-B-type branch            | same scramble as `EncTy_SB` in `inst[31:0]` (the low 32 bits of the 48-bit instruction)                                                              | 13 (signed, 2-byte aligned) |
| `EncTy_QC_EJ` | Xqci E-J-type call              | `X[31:16]` → `inst[63:48]`<br>`X[12]` → `inst[31]`<br>`X[10:5]` → `inst[30:25]`<br>`X[15:13]` → `inst[20:17]`<br>`X[4:1]` → `inst[11:8]`<br>`X[11]` → `inst[7]` | 32 (signed) |
| `EncTy_QC_EAI`| Xqci E-AI-type absolute imm     | `X[31:0]` → `inst[47:16]`                                                                                                                             | 32 (signed) |

---

## Standard Relocations

### Dynamic / Runtime Relocations

These are written by the dynamic linker at load time.

| Relocation | Type | Encoding | Operation | Size |
|------------|------|----------|-----------|------|
| `R_RISCV_NONE` | 0 | none | — | — |
| `R_RISCV_32` | 1 | `EncTy_32` | `S + A` | 32 |
| `R_RISCV_64` | 2 | `EncTy_64` | `S + A` | 64 |
| `R_RISCV_RELATIVE` | 3 | none | `B + A` | 32 |
| `R_RISCV_COPY` | 4 | none | Copy from shared object | 32 |
| `R_RISCV_JUMP_SLOT` | 5 | none | PLT entry | 32 |
| `R_RISCV_TLS_DTPMOD32` | 6 | none | TLS module index (32-bit) | 32 |
| `R_RISCV_TLS_DTPMOD64` | 7 | none | TLS module index (64-bit) | 64 |
| `R_RISCV_TLS_DTPREL32` | 8 | `EncTy_32` | TLS DTP offset (32-bit) | 32 |
| `R_RISCV_TLS_DTPREL64` | 9 | `EncTy_64` | TLS DTP offset (64-bit) | 64 |
| `R_RISCV_TLS_TPREL32` | 10 | `EncTy_32` | TLS TP offset (32-bit) | 32 |
| `R_RISCV_TLS_TPREL64` | 11 | `EncTy_64` | TLS TP offset (64-bit) | 64 |

### Control Flow Relocations

| Relocation | Type | Encoding | Operation | Inst size | Alignment | Range check |
|------------|------|----------|-----------|-----------|-----------|-------------|
| `R_RISCV_BRANCH` | 12 | `EncTy_SB` | `S + A - P` | 32-bit | 2 bytes | `−4096 <= X < 4096`, `X` must be even |
| `R_RISCV_JAL` | 13 | `EncTy_UJ` | `S + A - P` | 32-bit | 2 bytes | `−1048576 <= X < 1048576`, `X` must be even |
| `R_RISCV_CALL` | 14 | `EncTy_None` | `S + A - P` | 32-bit | 2 bytes | none (32-bit range via `auipc+jalr`) |
| `R_RISCV_CALL_PLT` | 15 | `EncTy_None` | `S + A - P` (via PLT) | 32-bit | 2 bytes | none (32-bit range via `auipc+jalr`) |
| `R_RISCV_RVC_BRANCH` | 44 | `EncTy_CB` | `S + A - P` | 16-bit | 2 bytes | `−256 <= X < 256`, `X` must be even |
| `R_RISCV_RVC_JUMP` | 45 | `EncTy_CJ` | `S + A - P` | 16-bit | 2 bytes | `−2048 <= X < 2048`, `X` must be even |

> **`R_RISCV_CALL` / `R_RISCV_CALL_PLT`:** Applied to an `auipc`+`jalr` pair
> (8 bytes). The linker splits `X` into a 20-bit upper part (encoded in `auipc`)
> and a 12-bit signed lower part (encoded in `jalr`), with a carry adjustment
> when bit 11 of the lower part is set. These relocations are the primary
> candidates for call relaxation (see [Relaxations](#relaxations)).

### PC-Relative Address Relocations

These always appear in pairs: a HI20 relocation on `auipc` followed by a
LO12 relocation on the instruction that uses the result.

| Relocation | Type | Encoding | Operation | Inst size | Range check |
|------------|------|----------|-----------|-----------|-------------|
| `R_RISCV_PCREL_HI20` | 19 | `EncTy_U_HI20` | `S + A - P` | 32-bit | none |
| `R_RISCV_PCREL_LO12_I` | 20 | `EncTy_I` | low 12 bits of paired HI20 | 32-bit | none |
| `R_RISCV_PCREL_LO12_S` | 21 | `EncTy_S` | low 12 bits of paired HI20 (stores) | 32-bit | none |

> **Pair invariant:** The symbol of `R_RISCV_PCREL_LO12_I/S` must point to
> the address of the corresponding `R_RISCV_PCREL_HI20` relocation site, not
> the target symbol. The linker resolves the HI20 first, then propagates its
> computed value to the LO12.

### Absolute Address Relocations

| Relocation | Type | Encoding | Operation | Inst size | Range check |
|------------|------|----------|-----------|-----------|-------------|
| `R_RISCV_HI20` | 22 | `EncTy_U_HI20` | `S + A` | 32-bit | `−2^31 <= X < 2^31` (32-bit range) |
| `R_RISCV_LO12_I` | 23 | `EncTy_I` | `S + A` low 12 bits | 32-bit | none |
| `R_RISCV_LO12_S` | 24 | `EncTy_S` | `S + A` low 12 bits (stores) | 32-bit | none |

### TLS Relocations

| Relocation | Type | Encoding | Operation | Inst size |
|------------|------|----------|-----------|-----------|
| `R_RISCV_TPREL_HI20` | 25 | `EncTy_U_HI20` | `S + A - tp` | 32-bit |
| `R_RISCV_TPREL_LO12_I` | 26 | `EncTy_I` | low 12 bits of TP offset | 32-bit |
| `R_RISCV_TPREL_LO12_S` | 27 | `EncTy_S` | low 12 bits of TP offset (stores) | 32-bit |
| `R_RISCV_TPREL_ADD` | 28 | none | Relaxation hint (no bits written) | 32-bit |
| `R_RISCV_TLS_GOT_HI20` | 17 | `EncTy_U_HI20` | `GOT(S) - P` | 32-bit |
| `R_RISCV_TLS_GD_HI20` | 18 | `EncTy_U_HI20` | `GOT(S) - P` (GD model) | 32-bit |
| `R_RISCV_TLSDESC_HI20` | 58 | `EncTy_U_HI20` | `TLSDESC(S) - P` | 32-bit |
| `R_RISCV_TLSDESC_LOAD_LO12` | 59 | `EncTy_I` | low 12 bits of TLSDESC | 32-bit |
| `R_RISCV_TLSDESC_ADD_LO12` | 60 | `EncTy_I` | low 12 bits of TLSDESC | 32-bit |
| `R_RISCV_TLSDESC_CALL` | 61 | none | TLSDESC call hint | 32-bit |

### GOT Relocations

| Relocation | Type | Encoding | Operation | Inst size |
|------------|------|----------|-----------|-----------|
| `R_RISCV_GOT_HI20` | 16 | `EncTy_U_HI20` | `GOT(S) - P` | 32-bit |
| `R_RISCV_GOT32_PCREL` | 37 | `EncTy_None` | `GOT(S) - P` (32-bit) | 32-bit |

### Arithmetic / Addend Relocations

Used in debug sections and compact unwind tables to encode label differences.

| Relocation | Type | Encoding | Operation | Size |
|------------|------|----------|-----------|------|
| `R_RISCV_ADD8` | 29 | `EncTy_8` | `*(loc) += S + A` | 8 |
| `R_RISCV_ADD16` | 30 | `EncTy_16` | `*(loc) += S + A` | 16 |
| `R_RISCV_ADD32` | 31 | `EncTy_32` | `*(loc) += S + A` | 32 |
| `R_RISCV_ADD64` | 32 | `EncTy_64` | `*(loc) += S + A` | 64 |
| `R_RISCV_SUB8` | 33 | `EncTy_8` | `*(loc) -= S + A` | 8 |
| `R_RISCV_SUB16` | 34 | `EncTy_16` | `*(loc) -= S + A` | 16 |
| `R_RISCV_SUB32` | 35 | `EncTy_32` | `*(loc) -= S + A` | 32 |
| `R_RISCV_SUB64` | 36 | `EncTy_64` | `*(loc) -= S + A` | 64 |
| `R_RISCV_SUB6` | 52 | `EncTy_6` | `*(loc)[5:0] -= S + A` | 8 |
| `R_RISCV_SET6` | 53 | `EncTy_6` | `*(loc)[5:0] = S + A` | 8 |
| `R_RISCV_SET8` | 54 | `EncTy_8` | `*(loc) = S + A` | 8 |
| `R_RISCV_SET16` | 55 | `EncTy_16` | `*(loc) = S + A` | 16 |
| `R_RISCV_SET32` | 56 | `EncTy_32` | `*(loc) = S + A` | 32 |
| `R_RISCV_32_PCREL` | 57 | `EncTy_32` | `S + A - P` (32-bit) | 32 |
| `R_RISCV_SET_ULEB128` | 62 | `EncTy_LEB128` | `*(loc) = S + A` (ULEB128) | variable |
| `R_RISCV_SUB_ULEB128` | 63 | `EncTy_LEB128` | `*(loc) -= S + A` (ULEB128) | variable |

### Relaxation Hint Relocations

These carry no data; they mark sites eligible for relaxation.

| Relocation | Type | Meaning |
|------------|------|---------|
| `R_RISCV_RELAX` | 51 | Paired with another relocation to indicate the site may be relaxed |
| `R_RISCV_ALIGN` | 43 | Requests NOP padding to meet an alignment requirement; bytes may be deleted by relaxation |
| `R_RISCV_TPREL_ADD` | 28 | Marks the `add` in a `TPREL_HI20`+`add`+`TPREL_LO12` TLS-LE sequence; deleted during relaxation |
| `R_RISCV_VENDOR` | 255 | Vendor extension marker; the following relocation is vendor-defined |

---

## Internal (Linker-Only) Relocations

These relocations are created by ELD during relaxation and are never
present in input object files. They are used to apply the final encoding
after an instruction sequence has been rewritten.

| Relocation | Encoding | Operation | Inst size | Notes |
|------------|----------|-----------|-----------|-------|
| `R_RISCV_RVC_LUI` | `EncTy_CI` | `S` | 16-bit | LUI compressed to `c.lui` |
| `R_RISCV_RVC_LI` | `EncTy_CI` | `S` | 16-bit | Load immediate compressed to `c.li` |
| `R_RISCV_GPREL_I` | `EncTy_I` | `S - G` | 32-bit | GP-relative I-type load/compute |
| `R_RISCV_GPREL_S` | `EncTy_S` | `S - G` | 32-bit | GP-relative S-type store |
| `R_RISCV_TPREL_I` | `EncTy_I` | `S - tp` | 32-bit | TP-relative I-type after TLS-LE relaxation |
| `R_RISCV_TPREL_S` | `EncTy_S` | `S - tp` | 32-bit | TP-relative S-type after TLS-LE relaxation |
| `R_RISCV_TBJAL` | none | — | 16-bit | Table-jump (`cm.jt`/`cm.jalt`) after call relaxation |
| `R_RISCV_QC_ABS26_I` | `EncTy_QC_EI` | `S + A` | 48-bit | Xqcilo absolute load (26-bit imm) |
| `R_RISCV_QC_ABS26_S` | `EncTy_QC_ES` | `S + A` | 48-bit | Xqcilo absolute store (26-bit imm) |
| `R_RISCV_QC_GPREL26_I` | `EncTy_QC_EI` | `S - G` | 48-bit | Xqcilo GP-relative load |
| `R_RISCV_QC_GPREL26_S` | `EncTy_QC_ES` | `S - G` | 48-bit | Xqcilo GP-relative store |
| `R_RISCV_QC_ABS20_U` | `EncTy_U_ABS20` | `S + A` | 32-bit | Xqci 20-bit absolute |
| `R_RISCV_QC_E_BRANCH` | `EncTy_QC_EB` | `S + A - P` | 48-bit | Xqci extended branch |
| `R_RISCV_QC_E_32` | `EncTy_QC_EAI` | `S + A` | 48-bit | Xqci extended 32-bit immediate |
| `R_RISCV_QC_E_CALL_PLT` | `EncTy_QC_EJ` | `S + A - P` | 48-bit | Xqci extended call (via PLT) |
| `R_RISCV_QC_ACCESS_16` | none | — | 16-bit | Xqcilo 16-bit access marker |
| `R_RISCV_QC_ACCESS_32` | none | — | 32-bit | Xqcilo 32-bit access marker |

---

## Relaxations

Relaxation is an iterative pass that shrinks instruction sequences when the
final link addresses allow it. Each pass may reduce code size; the linker
repeats until no further changes occur.

### Relaxation Flags

| Flag | Default | Description |
|------|---------|-------------|
| `--relax` / `--no-relax` | **enabled** | Master switch; disables all relaxations when off |
| `--no-relax-c` | off | Disable compressed-instruction relaxations (`c.j`, `c.jal`, `c.lui`, `c.li`) |
| `--no-relax-gp` | off | Disable GP-relative relaxations (requires `__global_pointer$`, non-PIC only) |
| `--no-relax-zero` | off | Disable zero-page relaxations (symbols with value in signed 12-bit range) |
| `--no-relax-got` | off | Disable GOT load relaxations for non-preemptible symbols |
| `--no-relax-tlsdesc` | off | Disable TLS descriptor relaxations |
| `--relax-xqci` / `--no-relax-xqci` | **disabled** | Enable Xqci instruction relaxations (`qc.e.j`, `qc.e.jal`) |
| `--relax-tbljal[=zcmt\|xqccmt]` / `--no-relax-tbljal` | **disabled** | Enable table-jump relaxations (`cm.jt`/`cm.jalt`) |

### Call Relaxation (`R_RISCV_CALL` / `R_RISCV_CALL_PLT`)

An `auipc`+`jalr` pair (8 bytes) is shrunk in priority order.

**→ `c.j` / `c.jal` (−6 bytes):** offset fits 12 bits signed, `--no-relax-c`
not set; `c.j` when `rd=x0`, `c.jal` when `rd=x1` (RV32 only).

```asm
# Before (8 bytes)
auipc  ra, %pcrel_hi(func)     # R_RISCV_CALL_PLT
jalr   ra, %pcrel_lo(1b)(ra)

# After (2 bytes)
c.jal  func                    # or c.j func  when rd=x0
```

**→ `cm.jt` / `cm.jalt` (−6 bytes):** `--relax-tbljal` enabled, target has a
valid JVT entry, non-PIC.

```asm
# Before (8 bytes)
auipc  ra, %pcrel_hi(func)     # R_RISCV_CALL_PLT
jalr   ra, %pcrel_lo(1b)(ra)

# After (2 bytes)
cm.jalt  <jvt_index>           # or cm.jt when rd=x0
```

**→ `jal` (−4 bytes):** offset fits 21 bits signed (±1 MiB).

```asm
# Before (8 bytes)
auipc  ra, %pcrel_hi(func)     # R_RISCV_CALL_PLT
jalr   ra, %pcrel_lo(1b)(ra)

# After (4 bytes)
jal    ra, func                # R_RISCV_JAL
```

**→ `qc.e.j` / `qc.e.jal` (−2 bytes):** `--relax-xqci` enabled, RV32 only.

```asm
# Before (8 bytes)
auipc  ra, %pcrel_hi(func)
jalr   ra, %pcrel_lo(1b)(ra)

# After (6 bytes)
qc.e.jal  func                 # 48-bit Xqci extended call
```

### JAL Relaxation (`R_RISCV_JAL`)

**→ `cm.jt` / `cm.jalt` (−2 bytes):** `--relax-tbljal` enabled, valid JVT
entry.

```asm
# Before (4 bytes)
jal    ra, func                # R_RISCV_JAL

# After (2 bytes)
cm.jalt  <jvt_index>
```

### LUI Relaxation (`R_RISCV_HI20` + `R_RISCV_LO12_*`)

A `lui`+LO12 pair is shrunk in priority order.

**→ zero-base (−4 bytes):** symbol value fits signed 12 bits; `--no-relax-zero`
not set.

```asm
# Before (8 bytes)
lui   a0, %hi(var)             # R_RISCV_HI20
addi  a0, a0, %lo(var)         # R_RISCV_LO12_I

# After (4 bytes)
addi  a0, x0, var              # immediate encodes full value
```

**→ GP-relative (−4 bytes):** symbol within `gp ± 2048`; `--no-relax-gp` not
set, non-PIC.

```asm
# Before (8 bytes)
lui   a0, %hi(var)             # R_RISCV_HI20
lw    a0, %lo(var)(a0)         # R_RISCV_LO12_I

# After (4 bytes)
lw    a0, var(gp)              # R_RISCV_GPREL_I
```

**→ `c.lui` (−2 bytes):** immediate fits 6-bit non-zero CI field; `rd ≠ x0`,
`rd ≠ x2`; `--no-relax-c` not set.

```asm
# Before (4 bytes)
lui   a0, %hi(var)             # R_RISCV_HI20

# After (2 bytes)
c.lui a0, %hi(var)             # R_RISCV_RVC_LUI
```

### PC-Relative to GP Relaxation (`R_RISCV_PCREL_HI20` + `R_RISCV_PCREL_LO12_I`)

**→ GP-relative (−4 bytes):** symbol within `gp ± 2048`; `--no-relax-gp` not
set, non-PIC.

```asm
# Before (8 bytes)
auipc a0, %pcrel_hi(var)       # R_RISCV_PCREL_HI20
lw    a0, %pcrel_lo(1b)(a0)    # R_RISCV_PCREL_LO12_I

# After (4 bytes)
lw    a0, var(gp)              # R_RISCV_GPREL_I
```

### GOT Relaxation (`R_RISCV_GOT_HI20` + `R_RISCV_PCREL_LO12_I`)

For non-preemptible symbols the GOT indirection is bypassed.

**→ `c.li` (−6 bytes):** symbol value fits 6-bit signed CI field.

```asm
# Before (8 bytes)
auipc a0, %pcrel_hi(sym@GOT)   # R_RISCV_GOT_HI20
lw    a0, %pcrel_lo(1b)(a0)    # R_RISCV_PCREL_LO12_I  (GOT load)

# After (2 bytes)
c.li  a0, sym                  # R_RISCV_RVC_LI
```

**→ `addi x0, imm` (−4 bytes):** symbol value fits 12-bit signed.

```asm
# Before (8 bytes)
auipc a0, %pcrel_hi(sym@GOT)
lw    a0, %pcrel_lo(1b)(a0)

# After (4 bytes)
addi  a0, x0, sym              # immediate encodes full value
```

**→ `auipc` + `addi` PCREL (0 bytes):** symbol fits PC-relative 32-bit range;
GOT load converted to direct address calculation.

```asm
# Before (8 bytes)
auipc a0, %pcrel_hi(sym@GOT)   # R_RISCV_GOT_HI20
lw    a0, %pcrel_lo(1b)(a0)    # GOT load

# After (8 bytes)
auipc a0, %pcrel_hi(sym)       # R_RISCV_PCREL_HI20
addi  a0, a0, %pcrel_lo(1b)    # R_RISCV_PCREL_LO12_I  (no GOT access)
```

### TLSDESC Relaxation

For symbols with local/static TLS, the TLSDESC call sequence is replaced
with a direct TP-relative access. Disabled by `--no-relax-tlsdesc`.

```asm
# Before: GD/TLSDESC (16 bytes, calls runtime)
auipc  a0, %tlsdesc_hi(x)      # R_RISCV_TLSDESC_HI20
lw     t0, %tlsdesc_lo(1b)(a0) # R_RISCV_TLSDESC_LOAD_LO12
addi   a0, a0, %tlsdesc_lo(1b) # R_RISCV_TLSDESC_ADD_LO12
jalr   t0, t0(a0)              # R_RISCV_TLSDESC_CALL

# After: TLS-LE (8–12 bytes, no runtime call)
lui    a0, %tprel_hi(x)        # R_RISCV_TPREL_HI20   (omitted if fits 12-bit)
add    a0, a0, tp              # R_RISCV_TPREL_ADD
lw     a0, %tprel_lo(x)(a0)    # R_RISCV_TPREL_LO12_I
```

### Zcmt / Xqccmt Table-Jump Relaxation (`--relax-tbljal`)

The Zcmt extension introduces a 64-entry Jump Vector Table (`.riscv.jvt`),
a 64-byte aligned array of function-pointer-sized words. At link time ELD
builds this table from all call targets that appear more than once, then
replaces each call site with a 2-byte `cm.jalt N` (return via `ra`) or
`cm.jt N` (tail call, no link register written). The index `N` is the
zero-based slot in the JVT where the target address is stored.

Xqccmt uses the same 16-bit encoding under the names `qc.cm.jalt` /
`qc.cm.jt`, and additionally supports writing the return address into `t0`
(x5) by setting bit 0 in the JVT entry.

**`.riscv.jvt` section layout (4 entries shown, 32-bit target):**

```
.section .riscv.jvt,"a",@progbits
.align 6                         # 64-byte alignment (Zcmt requirement)
jvt_base:
  .word  foo                     # entry 0 — target of cm.jalt 0 / cm.jt 0
  .word  bar                     # entry 1
  .word  baz                     # entry 2
  .word  qux                     # entry 3
  ...                            # up to 64 entries
```

**Call site relaxation (−6 bytes per site, from `auipc`+`jalr` to `cm.jalt`):**

```asm
# Before (8 bytes): called at many sites throughout the binary
auipc  ra, %pcrel_hi(foo)        # R_RISCV_CALL_PLT
jalr   ra, %pcrel_lo(1b)(ra)

# After (2 bytes): JVT entry 0 holds foo's address
cm.jalt  0                       # R_RISCV_TBJAL — loads PC from jvt_base[0], links ra
```

**Tail-call relaxation (−6 bytes, `cm.jt` when `rd=x0`):**

```asm
# Before (8 bytes)
auipc  x0, %pcrel_hi(baz)
jalr   x0, %pcrel_lo(1b)(x0)

# After (2 bytes): JVT entry 2 holds baz's address
cm.jt  2                         # tail call — no link register written
```

**JAL → `cm.jalt` (−2 bytes):**

```asm
# Before (4 bytes): already within ±1 MiB but target is in the JVT
jal    ra, bar                   # R_RISCV_JAL

# After (2 bytes)
cm.jalt  1                       # R_RISCV_TBJAL
```

The linker only builds JVT entries for targets where the total bytes saved
across all call sites exceeds the 4-byte cost of the JVT entry itself plus
any required padding to meet the 64-byte section alignment.

### ALIGN Relaxation (`R_RISCV_ALIGN`)

NOP padding inserted by the assembler to satisfy `.align` directives is
removed by the linker after relaxation shrinks preceding code. The linker
recalculates required padding and deletes any excess NOP bytes.

```asm
# Before (assembler-inserted NOPs, e.g. .align 4 after a 2-byte instruction)
c.j    target                  # 2 bytes
nop                            # 2 bytes — R_RISCV_ALIGN padding
nop                            # 2 bytes — R_RISCV_ALIGN padding
aligned_label:                 # now at 4-byte boundary

# After (if relaxation has already re-aligned the section)
c.j    target                  # 2 bytes
nop                            # 2 bytes — only one NOP needed
aligned_label:
```

---

## Common Instruction Sequences

### Absolute Address (HI20 + LO12)

```asm
lui   a0, %hi(symbol)        # R_RISCV_HI20
addi  a0, a0, %lo(symbol)    # R_RISCV_LO12_I
```

### PC-Relative Address (AUIPC + LO12)

```asm
auipc a0, %pcrel_hi(symbol)  # R_RISCV_PCREL_HI20
addi  a0, a0, %pcrel_lo(1b)  # R_RISCV_PCREL_LO12_I
```

### Function Call (CALL → relaxed to JAL or compressed)

```asm
auipc ra, %pcrel_hi(func)    # R_RISCV_CALL / R_RISCV_CALL_PLT
jalr  ra, %pcrel_lo(1b)(ra)  # (paired with auipc)
```

After relaxation (if target within ±1 MiB):

```asm
jal   ra, func               # R_RISCV_JAL
```

### TLS Local-Exec (TPREL)

```asm
lui   a0, %tprel_hi(x)       # R_RISCV_TPREL_HI20
add   a0, a0, tp, %tprel_add(x)  # R_RISCV_TPREL_ADD
lw    a0, %tprel_lo(x)(a0)   # R_RISCV_TPREL_LO12_I
```

---

## References

- RISC-V ELF psABI specification:
  https://github.com/riscv-non-isa/riscv-elf-psabi-doc/blob/master/riscv-elf.adoc
- RISC-V ISA specification:
  https://github.com/riscv/riscv-isa-manual
- RISC-V Compressed extension (RVC) — Volume I, Chapter 16
- Zcmt table-jump extension:
  https://github.com/riscv/riscv-isa-manual
