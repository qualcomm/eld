# x86-64 Relocation Reference

## Introduction

This document describes the x86-64 relocations handled by ELD, including
their encoding types, operations, range checks, and dynamic relocation
behaviour.

The official ABI specification is the System V AMD64 ABI:
https://gitlab.com/x86-psABIs/x86-64-ABI

---

## Relocation Conventions

| Symbol | Meaning |
|--------|---------|
| `S` | Runtime address of the referenced symbol |
| `A` | Relocation addend |
| `P` | Address of the relocation site |
| `B` | Base address of the shared object (for `RELATIVE`) |
| `GOT` | Address of the symbol's entry in the Global Offset Table |
| `PLT` | Address of the symbol's entry in the Procedure Linkage Table |
| `TP` | Thread pointer (base of the TLS block for the current thread) |
| `DTP` | Dynamic thread pointer (base of the TLS module block) |
| `TLS` | Size of the TLS template (used to compute `TPOFF`) |

---

## Encoding Types

x86-64 uses only flat-field encodings — no bit-scrambling.

| Encoding     | Value bits written      | Field size |
|--------------|-------------------------|------------|
| `EncTy_None` | none                    | —          |
| `EncTy_8`    | `X[7:0]` → `inst[7:0]`  | 8-bit      |
| `EncTy_16`   | `X[15:0]` → `inst[15:0]`| 16-bit     |
| `EncTy_32`   | `X[31:0]` → `inst[31:0]`| 32-bit     |
| `EncTy_64`   | `X[63:0]` → `inst[63:0]`| 64-bit     |

## Range Checks

Two distinct checks are applied, depending on the relocation:

**`VerifyRange` (hard error):** Enabled only for specific relocations
(see table). Checks whether the computed value `X = result >> Shift` fits
in the encoding field. Signed relocations use `llvm::isInt<N>`, unsigned
use `llvm::isUInt<N>`.

**Truncation check (`forceVerify`, diagnostic only):** Enabled when the
user passes `--verify-reloc <name>`. Checks whether the unsigned result
overflows the field width. No error is raised unless explicitly requested.

The table below lists the exact bounds for each relocation.

| Relocation | Encoding | Signed | `VerifyRange` | Effective bound |
|------------|----------|--------|---------------|-----------------|
| `R_X86_64_NONE` | `EncTy_None` | — | no | — |
| `R_X86_64_64` | `EncTy_64` | no | no | `X` truncated to 64 bits (no check) |
| `R_X86_64_32` | `EncTy_32` | no | no | `X` truncated to 32 bits; overflow possible |
| `R_X86_64_32S` | `EncTy_32` | yes | no | `X` truncated to 32 bits; sign-extended on load |
| `R_X86_64_16` | `EncTy_16` | no | no | `X` truncated to 16 bits; overflow possible |
| `R_X86_64_8` | `EncTy_8` | no | no | `X` truncated to 8 bits; overflow possible |
| `R_X86_64_PC32` | `EncTy_32` | no | no | `X` truncated to 32 bits |
| `R_X86_64_PC16` | `EncTy_16` | no | no | `X` truncated to 16 bits |
| `R_X86_64_PC8` | `EncTy_8` | no | no | `X` truncated to 8 bits |
| `R_X86_64_PC64` | `EncTy_64` | no | no | `X` truncated to 64 bits |
| `R_X86_64_PLT32` | `EncTy_32` | yes | no | `X` truncated to 32 bits |
| `R_X86_64_GOT32` | `EncTy_32` | no | no | `X` truncated to 32 bits |
| `R_X86_64_GOTPCREL` | `EncTy_32` | yes | no | `X` truncated to 32 bits |
| `R_X86_64_GOTPCRELX` | `EncTy_32` | yes | no | `X` truncated to 32 bits |
| `R_X86_64_REX_GOTPCRELX` | `EncTy_32` | yes | no | `X` truncated to 32 bits |
| `R_X86_64_TLSGD` | `EncTy_32` | yes | **yes** | `−2^31 ≤ X ≤ 2^31 − 1` |
| `R_X86_64_TLSLD` | `EncTy_32` | yes | **yes** | `−2^31 ≤ X ≤ 2^31 − 1` |
| `R_X86_64_DTPOFF32` | `EncTy_32` | yes | **yes** | `−2^31 ≤ X ≤ 2^31 − 1` |
| `R_X86_64_GOTTPOFF` | `EncTy_32` | yes | **yes** | `−2^31 ≤ X ≤ 2^31 − 1` |
| `R_X86_64_TPOFF32` | `EncTy_32` | yes | **yes** | `−2^31 ≤ X ≤ 2^31 − 1` |
| `R_X86_64_DTPOFF64` | `EncTy_64` | yes | no | `X` truncated to 64 bits |
| `R_X86_64_TPOFF64` | `EncTy_64` | yes | no | `X` truncated to 64 bits |
| `R_X86_64_DTPMOD64` | `EncTy_64` | no | no | filled by dynamic linker |

> **Why only TLS 32-bit relocations have `VerifyRange`:** These relocations
> encode offsets within a TLS block or a PC-relative offset to a GOT entry,
> both of which must fit in a sign-extended 32-bit immediate field in the
> instruction encoding. A value outside `[−2^31, 2^31 − 1]` would silently
> truncate and produce a wrong address at runtime. For general data and
> PC-relative relocations the ABI permits truncation; for TLS it is always
> an error.

> **Implicit truncation for unsigned relocations:** `R_X86_64_32`,
> `R_X86_64_16`, and `R_X86_64_8` silently truncate the result to the field
> width. Use `--verify-reloc R_X86_64_32` (etc.) to promote truncation to a
> diagnostic.

---

## Relocation Table

### Absolute Relocations

The computed value `S + A` is written into the relocation site.

| Relocation | Type | Encoding | Operation | Signed | Range check |
|------------|------|----------|-----------|--------|-------------|
| `R_X86_64_NONE` | 0 | `EncTy_None` | — | — | — |
| `R_X86_64_64` | 1 | `EncTy_64` | `S + A` | no | truncated to 64 bits |
| `R_X86_64_32` | 10 | `EncTy_32` | `S + A` | no | truncated to 32 bits |
| `R_X86_64_32S` | 11 | `EncTy_32` | `S + A` | **yes** | truncated to 32 bits, sign-extended on load |
| `R_X86_64_16` | 12 | `EncTy_16` | `S + A` | no | truncated to 16 bits |
| `R_X86_64_8` | 14 | `EncTy_8` | `S + A` | no | truncated to 8 bits |

> **`R_X86_64_32` vs `R_X86_64_32S`:** Both write 32 bits. `R_X86_64_32`
> zero-extends (result must fit in `[0, 2^32)`); `R_X86_64_32S` sign-extends
> (result must fit in `[-2^31, 2^31)`). The compiler emits `32S` for
> addresses that are sign-extended when loaded into 64-bit registers, such as
> data or code in the low 2 GiB.

### PC-Relative Relocations

The computed value `S + A - P` is written. `P` is the address of the
relocation site; the caller's instruction already adds `P` back at runtime,
so the net effect is a direct reference to `S + A`.

| Relocation | Type | Encoding | Operation | Signed | Range check |
|------------|------|----------|-----------|--------|-------------|
| `R_X86_64_PC32` | 2 | `EncTy_32` | `S + A - P` | no | truncated to 32 bits |
| `R_X86_64_PC16` | 13 | `EncTy_16` | `S + A - P` | no | truncated to 16 bits |
| `R_X86_64_PC8` | 15 | `EncTy_8` | `S + A - P` | no | truncated to 8 bits |
| `R_X86_64_PC64` | 24 | `EncTy_64` | `S + A - P` | no | truncated to 64 bits |
| `R_X86_64_PLT32` | 4 | `EncTy_32` | `PLT(S) + A - P` | **yes** | truncated to 32 bits |

> **`R_X86_64_PLT32`:** Used for function calls (`call foo`). If `foo` is
> non-preemptible the linker writes `S + A - P` directly. If `foo` is
> preemptible a PLT entry is created and the formula becomes
> `PLT(S) + A - P`.

### GOT-Relative Relocations

These embed the PC-relative offset to the symbol's GOT entry. At runtime
the instruction loads the actual symbol address from the GOT.

| Relocation | Type | Encoding | Operation | Signed | Range check |
|------------|------|----------|-----------|--------|-------------|
| `R_X86_64_GOT32` | 3 | `EncTy_32` | `GOT(S) + A` | no | truncated to 32 bits |
| `R_X86_64_GOTPCREL` | 9 | `EncTy_32` | `GOT(S) + A - P` | **yes** | truncated to 32 bits |
| `R_X86_64_GOTPCRELX` | 41 | `EncTy_32` | `GOT(S) + A - P` | **yes** | truncated to 32 bits |
| `R_X86_64_REX_GOTPCRELX` | 42 | `EncTy_32` | `GOT(S) + A - P` | **yes** | truncated to 32 bits |

> **`GOTPCRELX` / `REX_GOTPCRELX`:** Relaxable variants of `GOTPCREL`.
> `GOTPCRELX` is used without a REX prefix; `REX_GOTPCRELX` is used with
> one. When the symbol is non-preemptible the linker may rewrite the
> instruction sequence to avoid the GOT load entirely (see
> [Relaxations](#relaxations)).

### Dynamic / Runtime Relocations

Written by the dynamic linker at load time; not applied statically by ELD.

| Relocation | Type | Encoding | Operation |
|------------|------|----------|-----------|
| `R_X86_64_COPY` | 5 | `EncTy_None` | Copy symbol value from DSO into the executable |
| `R_X86_64_GLOB_DAT` | 6 | `EncTy_64` | `S` — resolve GOT entry to symbol address |
| `R_X86_64_JUMP_SLOT` | 7 | `EncTy_64` | `S` — lazy PLT resolution |
| `R_X86_64_RELATIVE` | 8 | `EncTy_64` | `B + A` — base-relative fixup for non-preemptible symbols in PIC |

### TLS Relocations

### General Dynamic (GD) — `R_X86_64_TLSGD`

Calls `__tls_get_addr` with a two-word GOT descriptor (module ID +
DTP offset). Works in all contexts.

| Relocation | Type | Encoding | Operation | Signed | Range check |
|------------|------|----------|-----------|--------|-------------|
| `R_X86_64_TLSGD` | 19 | `EncTy_32` | `GOT(S) + A - P` | **yes** | `−2^31 ≤ X ≤ 2^31 − 1` (hard error) |

```asm
leaq  x@tlsgd(%rip), %rdi      # R_X86_64_TLSGD; GOT holds {dtpmod, dtpoff}
call  __tls_get_addr@PLT
# %rax now holds &x in the current thread's TLS block
```

### Local Dynamic (LD) — `R_X86_64_TLSLD`

Like GD but for symbols local to the module; one `__tls_get_addr` call
covers all LD symbols in the module.

| Relocation | Type | Encoding | Operation | Signed | Range check |
|------------|------|----------|-----------|--------|-------------|
| `R_X86_64_TLSLD` | 20 | `EncTy_32` | `GOT(S) + A - P` | **yes** | `−2^31 ≤ X ≤ 2^31 − 1` (hard error) |

```asm
leaq  x@tlsld(%rip), %rdi      # R_X86_64_TLSLD; GOT holds {dtpmod, 0}
call  __tls_get_addr@PLT
# %rax is the base of this module's TLS block
movl  x@dtpoff(%rax), %eax     # R_X86_64_DTPOFF32: DTP offset added by linker
```

### Initial Exec (IE) — `R_X86_64_GOTTPOFF`

Loads the TP-relative offset from the GOT; no runtime call.

| Relocation | Type | Encoding | Operation | Signed | Range check |
|------------|------|----------|-----------|--------|-------------|
| `R_X86_64_GOTTPOFF` | 22 | `EncTy_32` | `GOT(S) + A - P` | **yes** | `−2^31 ≤ X ≤ 2^31 − 1` (hard error) |

```asm
movq  x@gottpoff(%rip), %rax   # R_X86_64_GOTTPOFF; GOT[x] = tpoff(x)
movl  %fs:(%rax), %eax         # load x relative to TP
```

### Local Exec (LE) — `R_X86_64_TPOFF32` / `R_X86_64_TPOFF64`

Encodes the TP-relative offset directly; fastest model, only valid in
executables.

| Relocation | Type | Encoding | Operation | Signed | Range check |
|------------|------|----------|-----------|--------|-------------|
| `R_X86_64_TPOFF32` | 23 | `EncTy_32` | `S + A - TLS` | **yes** | `−2^31 ≤ X ≤ 2^31 − 1` (hard error) |
| `R_X86_64_TPOFF64` | 18 | `EncTy_64` | `S + A - TLS` | **yes** | truncated to 64 bits |

```asm
movl  %fs:x@tpoff, %eax        # R_X86_64_TPOFF32: offset baked in at link time
```

### DTP Offset Relocations

Used inside GOT entries created for GD/LD access.

| Relocation | Type | Encoding | Operation | Signed | Range check |
|------------|------|----------|-----------|--------|-------------|
| `R_X86_64_DTPMOD64` | 16 | `EncTy_64` | TLS module index (filled by dynamic linker) | no | — |
| `R_X86_64_DTPOFF64` | 17 | `EncTy_64` | `S + A` — DTP-relative offset | **yes** | truncated to 64 bits |
| `R_X86_64_DTPOFF32` | 21 | `EncTy_32` | `S + A` — DTP-relative offset (32-bit) | **yes** | `−2^31 ≤ X ≤ 2^31 − 1` (hard error) |

---

## Dynamic Relocation Behaviour

ELD emits dynamic relocations during link when static resolution is not
possible. The table below summarises when each dynamic relocation type is
created.

| Dynamic relocation | Trigger condition |
|--------------------|------------------|
| `R_X86_64_RELATIVE` | Non-preemptible symbol referenced by `R_X86_64_64` in PIC/PIE; or non-preemptible symbol referenced by `GOTPCREL*` |
| `R_X86_64_GLOB_DAT` | Preemptible symbol with a GOT entry (`GOTPCREL*`) |
| `R_X86_64_64` | Preemptible symbol referenced by `R_X86_64_64` in a shared object |
| `R_X86_64_JUMP_SLOT` | Symbol referenced via PLT (`PLT32`) in a shared object |
| `R_X86_64_COPY` | Global data symbol referenced from a non-PIC executable |
| `R_X86_64_TPOFF64` | TLS IE/LD symbol in a shared object (`GOTTPOFF`) |
| `R_X86_64_DTPMOD64` | TLS GD/LD first GOT entry (module ID) |
| `R_X86_64_DTPOFF64` | TLS GD second GOT entry for preemptible symbol |

---

## Relaxations

x86-64 supports GOT-load relaxation for `GOTPCRELX` and `REX_GOTPCRELX`
when the referenced symbol is non-preemptible. The linker rewrites the
instruction in-place; no separate flag is required.

### `GOTPCRELX` Relaxation (no REX prefix)

Applies to instructions that load or call through the GOT without a REX
prefix (typically 32-bit operand-size or short-form instructions).

**`call *foo@GOTPCREL(%rip)` → `call foo`**

```asm
# Before (FF /2 — indirect call through GOT, 6 bytes)
call  *foo@GOTPCREL(%rip)      # R_X86_64_GOTPCRELX
                               # encodes: FF 15 <pcrel32 to GOT slot>

# After (E8 — direct call, 5 bytes + 1 NOP)
call  foo                      # direct; R_X86_64_PC32 applied to offset
nop                            # 1-byte filler to preserve instruction length
```

**`jmp *foo@GOTPCREL(%rip)` → `jmp foo`**

```asm
# Before (FF /4 — indirect jump, 6 bytes)
jmp   *foo@GOTPCREL(%rip)      # R_X86_64_GOTPCRELX

# After (E9 — direct jump, 5 bytes + 1 NOP)
jmp   foo
nop
```

**`mov foo@GOTPCREL(%rip), %reg` → `lea foo(%rip), %reg`**

```asm
# Before: load address from GOT
movq  foo@GOTPCREL(%rip), %rax # R_X86_64_GOTPCRELX

# After: compute address directly (non-preemptible, no GOT needed)
leaq  foo(%rip), %rax          # R_X86_64_PC32
```

### `REX_GOTPCRELX` Relaxation (REX prefix present)

Applies to 64-bit forms of the same instructions (REX.W prefix).

**`movq foo@GOTPCREL(%rip), %reg` → `leaq foo(%rip), %reg`**

```asm
# Before (REX.W + 8B /5 — 64-bit GOT load, 7 bytes)
movq  foo@GOTPCREL(%rip), %rax # R_X86_64_REX_GOTPCRELX

# After (REX.W + 8D /5 — 64-bit LEA, 7 bytes)
leaq  foo(%rip), %rax          # R_X86_64_PC32
```

**`movq foo@GOTPCREL(%rip), %reg` → `movq $imm32, %reg`** (small absolute)

```asm
# Before (REX.W + 8B — GOT load, 7 bytes)
movq  foo@GOTPCREL(%rip), %rax # R_X86_64_REX_GOTPCRELX

# After (REX.W + C7 — sign-extended 32-bit immediate, 7 bytes)
movq  $foo, %rax               # R_X86_64_32S; valid when foo fits in 32 bits
```

---

## Common Instruction Sequences

### Absolute Address (64-bit)

```asm
movabsq  $symbol, %rax         # R_X86_64_64
```

### Absolute Address (32-bit zero-extend)

```asm
movl  $symbol, %eax            # R_X86_64_32; valid when symbol < 2^32
```

### PC-Relative Data Access

```asm
leaq  symbol(%rip), %rax       # R_X86_64_PC32
```

### Function Call (direct or via PLT)

```asm
call  foo                      # R_X86_64_PLT32
                               # linker uses direct offset if non-preemptible,
                               # PLT stub otherwise
```

### GOT-Indirect Load

```asm
movq  foo@GOTPCREL(%rip), %rax # R_X86_64_REX_GOTPCRELX (or GOTPCREL)
movq  (%rax), %rax             # dereference the loaded address
```

### TLS Local-Exec

```asm
movl  %fs:x@tpoff, %eax        # R_X86_64_TPOFF32
```

### TLS Initial-Exec

```asm
movq  x@gottpoff(%rip), %rax   # R_X86_64_GOTTPOFF
movl  %fs:(%rax), %eax
```

---

## References

- System V AMD64 ABI specification:
  https://gitlab.com/x86-psABIs/x86-64-ABI
- Intel 64 and IA-32 Architectures Software Developer Manuals:
  https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
