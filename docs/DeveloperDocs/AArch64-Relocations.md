# AArch64 Relocation Reference

## Introduction

AArch64 relocation operators such as `:abs_g0_nc:`, `:pg_hi21:`, and `:lo12:` instruct the assembler and linker how to encode symbol references into instruction immediate fields.

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

- Ordinary GOT-page/load references reserve a slot for any visibility, symbol type, or link mode.
  The global-bound static IFUNC scan path uses the PLT-associated GOT
  slot instead; local-bound symbols take the ordinary local scanner.
- `CONDBR19`, `JUMP26`, and `CALL26` use `Preemptible=true` without a `STT_FUNC`
  check; `TSTBR14` does not request an ordinary PLT. ADRP and absolute
  function-address references use `Preemptible=true` and `STT_FUNC`. PREL data references use
  it only for non-`DynObj` output; PIE has `DynObj` output kind.
- The static IFUNC scanner can warn and still reserve a PLT for invalid
  IFUNC address forms. MOVW forms and literal loads are not classified as
  valid IFUNC address sequences; `MOVW_UABS_G1_NC` and PREL data forms
  additionally have unsupported-IFUNC warnings.
- Only `CALL26`/`JUMP26` have range stubs. For `D = target + A - P`,
  the current predicate is `abs(D >> 2) < 2^25` (nominal ±128 MiB).
  It requests a stub at the negative architectural endpoint too.
  The relaxation pass skips undefined targets; the stub predicate also
  exempts undefined weak non-DSO targets without a PLT. The target can
  be a PLT address; neither visibility nor `STT_FUNC` adds a range test.
  PIC and non-PIC templates differ. `TSTBR14`/`CONDBR19` overflow is an error.
- TLS IE omits the GOT for **local-bound static** targets (instruction
  conversion); global-bound static targets still reserve an IE slot. Dynamic
  links reserve IE slots for both local and global targets. TLSDESC
  address/load/add references use an IE slot for static or non-preemptible
  executable targets; preemptible executable and shared-library targets
  use descriptors.
  `TLSDESC_CALL` itself reserves neither GOT nor ordinary function PLT.

Sources: [scanner](../../lib/Target/AArch64/AArch64Relocator.cpp),
[range stub](../../lib/Target/AArch64/AArch64FarcallStub.cpp).

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

## Group Relocations

### Unsigned Absolute Relocations

| Operator | Relocation | Operation | Inst | Bits | Range Check | GOT allocation | PLT allocation | Trampoline / range |
|-----------|------------|------------|------|------|-------------|-------------------------|-------------------------|--------------------|
| `:abs_g0:` | `MOVW_UABS_G0` | `S + A` | `movz` | `[15:0]` | `0 <= X < 2^16` | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `:abs_g0_nc:` | `MOVW_UABS_G0_NC` | `S + A` | `movk` | `[15:0]` | none | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `:abs_g1:` | `MOVW_UABS_G1` | `S + A` | `movz` | `[31:16]` | `0 <= X < 2^32` | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `:abs_g1_nc:` | `MOVW_UABS_G1_NC` | `S + A` | `movk` | `[31:16]` | none | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `:abs_g2:` | `MOVW_UABS_G2` | `S + A` | `movz` | `[47:32]` | `0 <= X < 2^48` | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `:abs_g2_nc:` | `MOVW_UABS_G2_NC` | `S + A` | `movk` | `[47:32]` | none | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `:abs_g3:` | `MOVW_UABS_G3` | `S + A` | `movz/movk` | `[63:48]` | none | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |

### Signed Absolute Relocations

| Operator | Relocation | Operation | Inst | Bits | Range Check | GOT allocation | PLT allocation | Trampoline / range |
|-----------|------------|------------|------|------|-------------|-------------------------|-------------------------|--------------------|
| `:abs_g0_s:` | `MOVW_SABS_G0` | `S + A` | `movz/movn` | `[15:0]` | `-2^16 <= X < 2^16` | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `:abs_g1_s:` | `MOVW_SABS_G1` | `S + A` | `movz/movn` | `[31:16]` | `-2^32 <= X < 2^32` | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `:abs_g2_s:` | `MOVW_SABS_G2` | `S + A` | `movz/movn` | `[47:32]` | `-2^48 <= X < 2^48` | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |

### Example: Building a 64-bit Constant

```asm
movz x1, #:abs_g3:u64
movk x1, #:abs_g2_nc:u64
movk x1, #:abs_g1_nc:u64
movk x1, #:abs_g0_nc:u64
```

---

## PC-Relative Address Relocations

| Operator | Relocation | Expression | Instruction | Encoded Bits | Range Check | Alignment | GOT allocation | PLT allocation | Trampoline / range |
|-----------|------------|------------|-------------|--------------|-------------|-----------|-------------------------|-------------------------|--------------------|
| implicit | `LD_PREL_LO19` | `S+A-P` | literal `ldr` | `[20:2]` | `-2^20 <= X < 2^20` | `X` must be a multiple of 4 | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| implicit | `ADR_PREL_LO21` | `S+A-P` | `adr` | `[20:0]` | `-2^20 <= X < 2^20` | none | <ul><li><code>S: J[g&amp;i]</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]</code></li><li><code>H: -</code></li></ul> | No |
| `:pg_hi21:` | `ADR_PREL_PG_HI21` | `Page(S+A)-Page(P)` | `adrp` | `[31:12]` | `-2^32 <= X < 2^32` | none | <ul><li><code>S: J[g&amp;i]</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]; I: J[p&amp;f] + J[g&amp;i]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]; I: L[p&amp;f] + R[g&amp;i]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `:pg_hi21_nc:` | `ADR_PREL_PG_HI21_NC` | `Page(S+A)-Page(P)` | `adrp` | `[31:12]` | none | none | <ul><li><code>S: J[g&amp;i]</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]; I: J[p&amp;f] + J[g&amp;i]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]; I: L[p&amp;f] + R[g&amp;i]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |

> **`LD_PREL_LO19` alignment:** The literal `ldr` instruction addresses 4-byte-aligned
> literals; `X` (`S+A-P`) must therefore be divisible by 4. The linker encodes `X >> 2`
> into the 19-bit field and emits a warning if `X & 3 != 0`.

### Typical ADRP + ADD Sequence

```asm
adrp x0, :pg_hi21:foo
add  x0, x0, #:lo12:foo
```

---

## Low 12-bit Relocations

| Operator | Relocation | Used By | Immediate Extracted | GOT allocation | PLT allocation | Trampoline / range |
|-----------|------------|----------|--------------------|-------------------------|-------------------------|--------------------|
| `:lo12:` | `ADD_ABS_LO12_NC` | `add` | `X[11:0]` | <ul><li><code>S: J[g&amp;i]</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]</code></li><li><code>H: -</code></li></ul> | No |
| `:lo12:` | `LDST8_ABS_LO12_NC` | byte loads/stores | `X[11:0]` | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `:lo12:` | `LDST16_ABS_LO12_NC` | halfword loads/stores | `X[11:1]` | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `:lo12:` | `LDST32_ABS_LO12_NC` | word loads/stores | `X[11:2]` | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `:lo12:` | `LDST64_ABS_LO12_NC` | doubleword loads/stores | `X[11:3]` | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `:lo12:` | `LDST128_ABS_LO12_NC` | 128-bit accesses | `X[11:4]` | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |

---

## Control Flow Relocations

| Relocation | Instruction | Expression | Encoded Bits | Reach | Alignment | Out-of-range | GOT allocation | PLT allocation | Trampoline / range |
|------------|-------------|------------|--------------|-------|-----------|--------------|-------------------------|-------------------------|--------------------|
| `TSTBR14` | `tbz`, `tbnz` | `S+A-P` | `[15:2]` | `-2^15 <= X < 2^15` | `X` must be a multiple of 4 | error | <ul><li><code>S: J[g&amp;i]</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]</code></li><li><code>H: -</code></li></ul> | No |
| `CONDBR19` | `b.<cond>` | `S+A-P` | `[20:2]` | `-2^20 <= X < 2^20` | `X` must be a multiple of 4 | error | <ul><li><code>S: J[g&amp;i]</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[g&amp;i]</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[g&amp;i]</code></li><li><code>H: L[p]</code></li></ul> | No |
| `JUMP26` | `b` | `S+A-P` | `[27:2]` | `-2^27 <= X < 2^27` | `X` must be a multiple of 4 | trampoline; overflow error with `--no-trampolines` | <ul><li><code>S: J[g&amp;i]</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[g&amp;i]</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[g&amp;i]</code></li><li><code>H: L[p]</code></li></ul> | Yes; ±128 MiB (see predicate) |
| `CALL26` | `bl` | `S+A-P` | `[27:2]` | `-2^27 <= X < 2^27` | `X` must be a multiple of 4 | trampoline; overflow error with `--no-trampolines` | <ul><li><code>S: J[g&amp;i]</code></li><li><code>D: J[p]</code></li><li><code>P: J[p]; I: J[p] + J[g&amp;i]</code></li><li><code>H: J[p]</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: L[p]</code></li><li><code>P: L[p]; I: L[p] + R[g&amp;i]</code></li><li><code>H: L[p]</code></li></ul> | Yes; ±128 MiB (see predicate) |

> **Alignment:** All four control-flow relocations encode `X >> 2` into their immediate field,
> so `X` (`S+A-P`) must be divisible by 4. The linker emits an error if `X & 3 != 0`.
>
> **Out-of-range (`JUMP26`/`CALL26`):** When the branch target is beyond ±128 MB the linker
> inserts a trampoline stub for range extension, including when the destination is a PLT entry.
> If trampolines are disabled with
> `--no-trampolines`, the relocation instead reports an overflow error. The displacement is
> required to be 4-byte aligned and fit in the signed 28-bit range.
> `TSTBR14` and `CONDBR19` have no trampoline support; exceeding their range is a hard error.

---

## Data, GOT, and TLS Reservation Reference

These additional scanned forms make the data and TLS reservation policies
explicit. The conditions describe valid input targets, not support for using
a TLS relocation against a function. `ABS16` is rejected for PIC output;
TLS LE ADD forms are accepted in PIE but not in a shared-library PIC link.
The static IFUNC entries here refer to the global-bound IFUNC scan path.

| Relocation | Kind / behavior | GOT allocation | PLT allocation | Trampoline / range |
|------------|-----------------|-------------------------|-------------------------|--------------------|
| `R_AARCH64_ABS16` | Absolute data field | <ul><li><code>S: J[g&amp;i]</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: X</code></li><li><code>H: X</code></li></ul> | No |
| `R_AARCH64_ABS32` | Absolute data field | <ul><li><code>S: J[g&amp;i]</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]; I: J[p&amp;f] + J[g&amp;i]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]; I: L[p&amp;f] + R[g&amp;i]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_AARCH64_ABS64` | Absolute data field | <ul><li><code>S: J[g&amp;i]</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]; I: J[p&amp;f] + J[g&amp;i]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]; I: L[p&amp;f] + R[g&amp;i]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_AARCH64_PREL16` | PC-relative data field | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_PREL32` | PC-relative data field | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_PREL64` | PC-relative data field | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_ADR_GOT_PAGE` | Address of symbol GOT page | <ul><li><code>S: J[g&amp;i]{a} + G[!(g&amp;i)]</code></li><li><code>D: G</code></li><li><code>P: G; I: J[g&amp;i]{a} + G[!(g&amp;i)]</code></li><li><code>H: G</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_LD64_GOT_LO12_NC` | Low offset for symbol GOT load | <ul><li><code>S: J[g&amp;i]{a} + G[!(g&amp;i)]</code></li><li><code>D: G</code></li><li><code>P: G; I: J[g&amp;i]{a} + G[!(g&amp;i)]</code></li><li><code>H: G</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_LD64_GOTPAGE_LO15` | GOT-page-relative symbol GOT load | <ul><li><code>S: J[g&amp;i]{a} + G[!(g&amp;i)]</code></li><li><code>D: G</code></li><li><code>P: G; I: J[g&amp;i]{a} + G[!(g&amp;i)]</code></li><li><code>H: G</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_TLSIE_ADR_GOTTPREL_PAGE21` | IE GOT page | <ul><li><code>S: O[g&amp;t]</code></li><li><code>D: O[t]</code></li><li><code>P: O[t]; I: O[g&amp;t]</code></li><li><code>H: O[t]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_TLSIE_LD64_GOTTPREL_LO12_NC` | IE GOT load | <ul><li><code>S: O[g&amp;t]</code></li><li><code>D: O[t]</code></li><li><code>P: O[t]; I: O[g&amp;t]</code></li><li><code>H: O[t]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_TLSDESC_ADR_PAGE21` | TLS descriptor page | <ul><li><code>S: O[t]</code></li><li><code>D: C[t&amp;p] + O[t&amp;!p]</code></li><li><code>P: C[t&amp;p] + O[t&amp;!p]; I: O[t]</code></li><li><code>H: C[t]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_TLSDESC_LD64_LO12` | TLS descriptor load | <ul><li><code>S: O[t]</code></li><li><code>D: C[t&amp;p] + O[t&amp;!p]</code></li><li><code>P: C[t&amp;p] + O[t&amp;!p]; I: O[t]</code></li><li><code>H: C[t]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_TLSDESC_ADD_LO12` | TLS descriptor low address | <ul><li><code>S: O[t]</code></li><li><code>D: C[t&amp;p] + O[t&amp;!p]</code></li><li><code>P: C[t&amp;p] + O[t&amp;!p]; I: O[t]</code></li><li><code>H: C[t]</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_TLSDESC_CALL` | TLS resolver call / relaxation marker | <ul><li><code>S: U</code></li><li><code>D: U</code></li><li><code>P: U</code></li><li><code>H: U</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_TLSLE_ADD_TPREL_HI12` | Direct TP-relative high part | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: X</code></li></ul> | No |
| `R_AARCH64_TLSLE_ADD_TPREL_LO12` | Direct TP-relative low part | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: X</code></li></ul> | No |
| `R_AARCH64_TLSLE_ADD_TPREL_LO12_NC` | Direct TP-relative low part | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: X</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: X</code></li></ul> | No |

---

## Common Prefix Cheat Sheet

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

## Large Memory Model Relocations

The small and medium code models typically use ADRP+ADD sequences and are limited by the reach of ADRP. Large memory models use MOVZ/MOVK relocation sequences to construct arbitrary 64-bit addresses.

| Relocation | Operator | Instruction | Bits Loaded | Range Check | GOT allocation | PLT allocation | Trampoline / range |
|------------|------------|------------|------------|------------|-------------------------|-------------------------|--------------------|
| `R_AARCH64_MOVW_UABS_G0` | `:abs_g0:` | `movz` | [15:0] | `0 <= X < 2^16` | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_MOVW_UABS_G0_NC` | `:abs_g0_nc:` | `movk` | [15:0] | none | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_MOVW_UABS_G1` | `:abs_g1:` | `movz` | [31:16] | `0 <= X < 2^32` | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_MOVW_UABS_G1_NC` | `:abs_g1_nc:` | `movk` | [31:16] | none | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_MOVW_UABS_G2` | `:abs_g2:` | `movz` | [47:32] | `0 <= X < 2^48` | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_MOVW_UABS_G2_NC` | `:abs_g2_nc:` | `movk` | [47:32] | none | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_MOVW_UABS_G3` | `:abs_g3:` | `movz/movk` | [63:48] | none | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |

### Example

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

### GOT-Based Large Model Relocations

| Relocation | Purpose | GOT allocation | PLT allocation | Trampoline / range |
|------------|---------|-------------------------|-------------------------|--------------------|
| `R_AARCH64_ADR_GOT_PAGE` | Compute GOT page address | <ul><li><code>S: J[g&amp;i]{a} + G[!(g&amp;i)]</code></li><li><code>D: G</code></li><li><code>P: G; I: J[g&amp;i]{a} + G[!(g&amp;i)]</code></li><li><code>H: G</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_LD64_GOT_LO12_NC` | Load GOT entry offset | <ul><li><code>S: J[g&amp;i]{a} + G[!(g&amp;i)]</code></li><li><code>D: G</code></li><li><code>P: G; I: J[g&amp;i]{a} + G[!(g&amp;i)]</code></li><li><code>H: G</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_GLOB_DAT` | Dynamic linker fills GOT entry | <ul><li><code>S: U</code></li><li><code>D: U</code></li><li><code>P: U</code></li><li><code>H: U</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_JUMP_SLOT` | PLT/GOT function resolution | <ul><li><code>S: U</code></li><li><code>D: U</code></li><li><code>P: U</code></li><li><code>H: U</code></li></ul> | <ul><li><code>S: U</code></li><li><code>D: U</code></li><li><code>P: U</code></li><li><code>H: U</code></li></ul> | No |

### Reach Comparison

| Scheme | Reach |
|---------|---------|
| `ADR` | Approximately ±1 MB |
| `ADRP + ADD` | Approximately ±4 GB |
| `MOVZ/MOVK + MOVW_UABS_G*` | Full 64-bit address space |
| GOT-based sequence | Full 64-bit address space |


---

## AArch64 Code Models

### Small Code Model

Assumptions:
- Code and static data are reachable using PC-relative addressing.
- Symbols are typically accessed using `ADRP + ADD` or `ADRP + LDR/STR`.

Common relocations:

| Relocation | Purpose | GOT allocation | PLT allocation | Trampoline / range |
|------------|---------|-------------------------|-------------------------|--------------------|
| `R_AARCH64_ADR_PREL_PG_HI21` | ADRP page calculation | <ul><li><code>S: J[g&amp;i]</code></li><li><code>D: J[p&amp;f]</code></li><li><code>P: J[p&amp;f]; I: J[p&amp;f] + J[g&amp;i]</code></li><li><code>H: J[p&amp;f]</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: L[p&amp;f]</code></li><li><code>P: L[p&amp;f]; I: L[p&amp;f] + R[g&amp;i]</code></li><li><code>H: L[p&amp;f]</code></li></ul> | No |
| `R_AARCH64_ADD_ABS_LO12_NC` | ADD low 12 bits | <ul><li><code>S: J[g&amp;i]</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_LDST8_ABS_LO12_NC` | 8-bit load/store offset | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_LDST16_ABS_LO12_NC` | 16-bit load/store offset | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_LDST32_ABS_LO12_NC` | 32-bit load/store offset | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_LDST64_ABS_LO12_NC` | 64-bit load/store offset | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |

Example:

```asm
adrp x0, :pg_hi21:global
add  x0, x0, #:lo12:global
```

### Medium Code Model

Assumptions:
- Code remains reachable through branch relocations.
- Data may reside outside the ADRP reach window.
- Large objects are frequently accessed through the GOT.

Common relocations:

| Relocation | Purpose | GOT allocation | PLT allocation | Trampoline / range |
|------------|---------|-------------------------|-------------------------|--------------------|
| `R_AARCH64_ADR_GOT_PAGE` | Compute GOT page | <ul><li><code>S: J[g&amp;i]{a} + G[!(g&amp;i)]</code></li><li><code>D: G</code></li><li><code>P: G; I: J[g&amp;i]{a} + G[!(g&amp;i)]</code></li><li><code>H: G</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_LD64_GOT_LO12_NC` | Access GOT entry | <ul><li><code>S: J[g&amp;i]{a} + G[!(g&amp;i)]</code></li><li><code>D: G</code></li><li><code>P: G; I: J[g&amp;i]{a} + G[!(g&amp;i)]</code></li><li><code>H: G</code></li></ul> | <ul><li><code>S: R[g&amp;i]</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_GLOB_DAT` | Dynamic linker fills GOT | <ul><li><code>S: U</code></li><li><code>D: U</code></li><li><code>P: U</code></li><li><code>H: U</code></li></ul> | <ul><li><code>S: -</code></li><li><code>D: -</code></li><li><code>P: -</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_JUMP_SLOT` | Function resolution via PLT/GOT | <ul><li><code>S: U</code></li><li><code>D: U</code></li><li><code>P: U</code></li><li><code>H: U</code></li></ul> | <ul><li><code>S: U</code></li><li><code>D: U</code></li><li><code>P: U</code></li><li><code>H: U</code></li></ul> | No |

Example:

```asm
adrp x0, :got:symbol
ldr  x0, [x0, #:got_lo12:symbol]
```

### Large Code Model

Assumptions:
- Code and data may be located anywhere in the 64-bit address space.
- Full-width addresses are materialized using MOVZ/MOVK relocation sequences.

Common relocations:

| Relocation | GOT allocation | PLT allocation | Trampoline / range |
|------------|-------------------------|-------------------------|--------------------|
| `R_AARCH64_MOVW_UABS_G0` | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_MOVW_UABS_G0_NC` | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_MOVW_UABS_G1` | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_MOVW_UABS_G1_NC` | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_MOVW_UABS_G2` | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_MOVW_UABS_G2_NC` | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |
| `R_AARCH64_MOVW_UABS_G3` | <ul><li><code>S: J[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: J[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | <ul><li><code>S: R[g&amp;i]{w}</code></li><li><code>D: -</code></li><li><code>P: -; I: R[g&amp;i]{w}</code></li><li><code>H: -</code></li></ul> | No |

Example:

```asm
movz x0, #:abs_g3:symbol
movk x0, #:abs_g2_nc:symbol
movk x0, #:abs_g1_nc:symbol
movk x0, #:abs_g0_nc:symbol
```


---

## References

### Official AArch64 ELF ABI Specification

#### Current Source (GitHub)

- ELF for the Arm® 64-bit Architecture (AArch64)
  - https://github.com/ARM-software/abi-aa/blob/main/aaelf64/aaelf64.rst

#### AAELF64 Repository Directory

- https://github.com/ARM-software/abi-aa/tree/main/aaelf64

### Recommended Sections

When implementing linker relocations, the most useful sections are:

- Relocation Processing
- Static AArch64 Relocations
- Dynamic Relocations
- GOT and PLT Relocations
- TLS Relocations
- Code Models
- Program Loading and Dynamic Linking
