# GNU IFunc support

GNU IFunc functionality enables a developer to provide multiple implementations
for a function and a resolver function that selects at runtime which implementation to use.
It is typically used for selecting the most optimized implementation for a given CPU / runtime.
The resolver function is called once at the start of the application runtime and then the
selected implementation is fixed. Resolver function is always called eagerly (at program start)
instead of lazily (when first needed).

Example:

```c
__attribute__((ifunc("foo_resolver")))
int foo(int);

int foo_impl(int u) {
  return u;
}

int (*foo_resolver())(int) {
  return foo_impl;
}

int bar(int u) {
  int v = foo(u); // Calls the function that is returned by foo_resolver
}
```

# Static linking

eld generates a PLT slot for each ifunc symbol. Each PLT slot has a corresponding
GOTPLT slot. This model is very similar to how preemptible dynamic symbols are handled.
However, instead of doing symbol resolution at runtime to fill the GOTPLT slot, the runtime
calls the corresponding resolver function to fill the GOTPLT slot.

For the case of static executables, libc plays the role of runtime and takes on a task
that is typically unusual for a libc -- resolve symbols and patch the binary at runtime
with the resolution information.

eld emits `R_<TARGET>_IRELATIVE` relocations in `.rel[a].plt` section, and `__rel[a]_iplt_start` and
`__rela_iplt_end` symbols that store the start and end addresses of `.rel[a].plt` section.

The tiny-loader in the libc process the `R_<TARGET>_IRELATIVE` relocations by
iterating over the [`__rel[a]_iplt_start`, `__rel[a]_iplt_end`) range.

```
Relocation section '.rela.plt' at offset 0x190 contains 6 entries:
    Offset             Info             Type               Symbol's Value
0000000000490000  0000000000000408 R_<TARGET>_IRELATIVE               4358c0
0000000000490008  0000000000000408 R_<TARGET>_IRELATIVE               40db20
```

The symbol value of `IRELATIVE` relocations contains the IFunc resolver address, and
the relocation offset points to the GOTPLT slot for the IFunc symbol. For each relocation,
the tiny-loader in the libc calls the IFunc resolver and stores the result in the GOTPLT slot.

**Note that there is no lazy binding here.**

### Direct reference to an IFunc symbol

Direct references to an IFunc symbol are resolved to the PLT slot of the IFunc symbol.

```
// global variable!
// Absolute relocation, for example, `R_AARCH64_ABS64`, `R_RISCV_64`.
int (*foo_gp)(int) = foo;
```

eld will resolve the absolute relocation to the address of PLT[foo].

## GOT references to an IFunc symbol

GOT references to an IFunc symbol are resolved to the GOTPLT slot of the IFunc symbol
when there are no direct references to the IFunc symbol. When there is a direct reference
to the IFunc symbol, then the GOT references to the IFunc symbols gets resolved to the
GOT slot of the IFunc symbol. It is to ensure pointer equality. Let's understand this
in detail:

Case 1: PIC code and no direct reference

```c
// foo is an ifunc symbol!
int main() {
  // adrp x0, :got:foo ;  R_AARCH64_GOT_PAGE
  // ldr x0, [x0, :got_lo12:foo] ; R_AARCH64_LD64_GOT_LO12_NC
  int (*foo_lp)(int) = foo;
}
```

Here eld will resolve `adrp + ldr` relocation pair to the address of GOTPLT[foo].
Hence, `ldr` will load the address that is stored in the GOTPLT[foo]. Hence,
`foo_lp` will store the address of the resolved function. With this design, there
is no indirection penalty for calls to `foo_lp`.

Case 2: PIC code and direct reference

```c
// foo is an ifunc symbol!

// R_AARCH64_ABS64
int (*foo_gp)(int) = foo;

int main() {
  // adrp x0, :got:foo ;  R_AARCH64_GOT_PAGE
  // ldr x0, [x0, :got_lo12:foo] ; R_AARCH64_LD64_GOT_LO12_NC
  int (*foo_lp)(int) = foo;
}
```

Here eld will resolve `ABS64` to the address of PLT[foo]. With this, calls to `foo_gp` will
work as expected. Note that we cannot resolve `ABS64` to the address of
the resolved function because at link time we cannot know the resolved function.

Now, if we resolve `adrp + ldr` relocation pair as before, then `foo_lp` will store the address of
the resolved function. This is a problem because `foo_gp` and `foo_lp`, both pointers to the same
function `foo`, have different values.

To resolve this, whenever their is a direct reference to an ifunc symbol `foo`,
eld creates an additional GOT slot for `foo`, and fill that with the address of
the PLT[foo], and resolve all GOT references of `foo` to the GOT[foo] instead of the GOTPLT[foo].
With this design, `foo_lp` will store the address of PLT[foo], the same as `foo_gp`. Hence,
no pointer inequality issue.

## IFunc behaviour across all relocations

To describe IFunc behavior for all relocations, we categorize the relocations
into the following categories:

- Absolute / PC-relative data relocations
- GOT-related data relocations
- Control flow relocations
- Regular GOT-related instruction relocations
- Regular absolute / PC-relative address-forming relocations
- Address forming relocations used to compute both GOT and non-GOT addresses.
- Absolute / PC-relative load/store relocations

*Regular* in the relocation category name here means that the relocations are
non-TLS non-call relocations.

Any relocation against an IFunc symbol that does not fall into one of the above
categories is invalid and produces a warning.

The relocations which are not supported by GNU for IFunc symbols are annotated with
NotSupportedInGNULDForIFunc. The GNU toolchain that is used for verifying this is:
aarch64-none-linux-gnu-gcc (Arm GNU Toolchain 15.2.Rel1 (Build arm-15.86)) 15.2.1 20251203,
and riscv64-unknown-linux-ld (GNU ld (GNU Binutils) 2.43.1).


### Absolute / PC-relative data relocations

Resolves to PLT[IFuncSymbol]. Sets HasDirectReference[IFuncSymbol] to true.

- R_AARCH64_ABS{16, 32, 64}

- R_RISCV_{32, 64}

- R_ARM_ABS32
- R_ARM_ABS32_NOI
- R_ARM_TARGET1 (treated as R_ARM_ABS32)

The below relocations are currently unsupported by eld.
For IFunc symbols, these should be resolved to the PLT[IFuncSymbol] as well.

- R_AARCH64_PREL{16, 32, 64} (NotSupportedInGNULDForIFunc)
- R_AARCH64_PLT32

- R_RISCV_PLT32

- R_ARM_ABS8
- R_ARM_ABS16
- R_ARM_PREL{16, 32, 64}

### GOT-related data relocations

The below relocations are currently unsupported by eld.

- R_AARCH64_GOTREL{32, 64}
- R_AARCH64_GOTPCREL32

- R_RISCV_GOT32_PCREL

### Control flow relocations

Resolves to PLT[IFuncSymbol].

- R_AARCH64_TSTBR14
- R_AARCH64_CONDBR19
- R_AARCH64_JUMP26
- R_AARCH64_CALL26

- R_RISCV_CALL
- R_RISCV_CALL_PLT
- R_RISCV_JAL
- R_RISCV_BRANCH
- R_RISCV_RVC_BRANCH
- R_RISCV_RVC_JUMP

- R_ARM_CALL
- R_ARM_JUMP24
- R_ARM_PC24
- R_ARM_PLT32
- R_ARM_THM_CALL
- R_ARM_THM_JUMP24

The below relocations should not be used with IFunc functions because
these relocations are used with thumb instructions that does not support
interworking (correctly transferring control between thumb-state and ARM-state)
and PLT slot is typically always in the ARM state.

- R_ARM_THM_JUMP6 (UNSUPPORTED) (NotSupportedInGNULDForIFunc)
- R_ARM_THM_JUMP19
- R_ARM_THM_JUMP11
- R_ARM_THM_JUMP8

### Regular GOT-related instruction relocations

Resolves-to / uses GOTPLT[IFuncSymbol] if there is no direct reference to
IFuncSymbol; otherwise uses GOT[IFuncSymbol].

- R_AARCH64_ADR_GOT_PAGE
- R_AARCH64_LD{32,64}_GOT_LO12_NC (LD32 variant UNSUPPORTED)
- R_AARCH64_LD{32,64}_GOTPAGE_LO15 (LD32 variant UNSUPPORTED)

- R_RISCV_GOT_HI20

- R_ARM_GOT_BREL
- R_ARM_GOT_PREL

The below relocations are unsupported by eld:

- R_AARCH64_GOT_LD_PREL19
- AUTH-ABI GOT relocations
- R_AARCH64_MOVW_GOTOFF_G{0,1}{_NC}

- R_ARM_GOT_ABS

### Regular absolute / PC-relative address-forming relocations

Resolves to PLT[IFuncSymbol]. Sets HasDirectReference[IFuncSymbol] to true.

- R_AARCH64_ADR_PREL_LO21 (NotSupportedInGNULDForIFunc)
- R_AARCH64_ADR_PREL_PG_HI21{_NC}
- R_AARCH64_ADD_ABS_LO12_NC

- R_RISCV_HI20
- R_RISCV_PCREL_HI20
- R_RISCV_LO12_I
- R_RISCV_LO12_S

- R_ARM_REL32
- R_ARM_PREL31
- R_ARM_MOVW_ABS_NC
- R_ARM_MOVT_ABS
- R_ARM_MOVW_PREL_NC
- R_ARM_MOVT_PREL
- R_ARM_THM_MOVW_ABS_NC
- R_ARM_THM_MOVT_ABS
- R_ARM_THM_MOVW_PREL_NC
- R_ARM_THM_MOVT_PREL

The below relocations are resolved to the IFunc symbol:

- R_AARCH64_MOVW_UABS_G{0,1,2,3}{_NC} (NotSupportedInGNULDForIFunc)
- R_AARCH64_SABS_G{0,1,2,3} (NotSupportedInGNULDForIFunc)

> [!IMPORTANT]
> FIXME: We should perhaps resolve these relocations to the PLT[IFuncSymbol]
> instead of the IFuncSymbol for ensuring pointer equality.

The below relocations are currently unsupported in eld:

- R_AARCH64_MOVW_PREL_G{0, 1, 2, 3}{_NC}

### Address forming relocations used to compute both GOT and non-GOT addresses

These are the relocations that can be used to form both
GOT and non-GOT addresses.

The behavior of these relocations depend upon the corresponding Hi-relocation pair.

- R_RISCV_PCREL_LO12_I
- R_RISCV_PCREL_LO12_S

- There is no relevant AArch64 relocation here.

### Absolute / PC-relative load/store relocations

These relocations do not make sense with IFunc symbols. Loading a value at a
function address is invalid behavior, and so is storing a value at a function
address.

- LD_PREL_LO19 (NotSupportedInGNULDForIFunc)
- LDST{8, 16, 32, 64, 128}_ABS_LO12_NC (NotSupportedInGNULDForIFunc)

> [!IMPORTANT]
> FIXME: It should be an error to use these relocations with IFunc symbols.


