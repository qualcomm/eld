# GNU IFunc support with AArch64

GNU IFunc functionality enables a developer to provide multiple implementations
of a function and a resolver function that selects which implementation to use at runtime.
It is typically used for selecting the most optimized implementation for a given CPU / runtime.
The resolver function is called once at the start of the application runtime and then the
selected implementation is fixed.

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

## Static linking

eld generates a PLT slot for each ifunc symbol. Each PLT slot has a corresponding
GOTPLT slot. This model is very similar to how preemptible dynamic symbols are handled.
However, instead of doing symbol resolution at runtime to fill the GOTPLT slot, the runtime
calls the corresponding resolver function to fill the GOTPLT slot.

For the case of static executables, libc plays the role of runtime and takes on a task that is typically
unusual for a libc -- resolve symbols and patch the binary at runtime with the resolution information.

eld emits `R_AARCH64_IRELATIVE` relocations in `.rela.plt` section, and `__rela_iplt_start` and
`__rela_iplt_end` symbols that store the start and end addresses of `.rela.plt` section.

The tiny-loader in the libc process the `R_AARCH64_IRELATIVE` relocations by
iterating over the [`__rela_iplt_start`, `__rela_iplt_end`) range.

```
Relocation section '.rela.plt' at offset 0x190 contains 6 entries:
    Offset             Info             Type               Symbol's Value
0000000000490000  0000000000000408 R_AARCH64_IRELATIVE               4358c0
0000000000490008  0000000000000408 R_AARCH64_IRELATIVE               40db20
```

The symbol value of `IRELATIVE` relocations contains the IFunc resolver address, and
the relocation offset points to the GOTPLT slot for the IFunc symbol. For each relocation,
the tiny-loader in the libc calls the IFunc resolver and stores the result in the GOTPLT slot.

Note that there is no lazy binding here.

### Direct reference to an IFunc symbol

Direct references to an IFunc symbol are resolved to the PLT slot of the IFunc symbol.

```
// global variable!
// R_AARCH64_ABS64
int (*foo_gp)(int) = foo;
```

eld will resolve `R_AARCH64_ABS64` relocation to the address of PLT[foo].

### GOT references to an IFunc symbol

GOT references to an IFunc symbol are resolved to the GOTPLT slot of the IFunc symbol
when there is no direct references to the IFunc symbol. When there is a direct reference
to the IFunc symbol, then the GOT references to the IFunc symbols gets resolved to the
GOT slot of the IFunc symbol.

Let's see why:

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

### IFunc behaviour across all relocations

To describe IFunc behavior for all relocations, we categorize the relocations
into the following categories:

- Absolute / PC-relative data relocations
- GOT-related data relocations
- Control flow relocations
- GOT-related instruction relocations
- Absolute / PC-relative address-forming relocations
- Absolute / PC-relative load/store relocations.
- General computation relocation

The relocations which are not supported by GNU for IFunc symbols are annotated with
NotSupportedInGNULDForIFunc. The GNU toolchain that is used for verifying this is:
aarch64-none-linux-gnu-gcc (Arm GNU Toolchain 15.2.Rel1 (Build arm-15.86)) 15.2.1 20251203


#### Absolute / PC-relative data relocations

Resolves to PLT[IFuncSymbol]. Sets HasDirectReference[IFuncSymbol] to true.

- R_AARCH64_ABS{16, 32, 64}

Not handled currently.

[!IMPORTANT]
They should be resolved to PLT[IFuncSymbol] as well!

- R_AARCH64_PREL{16, 32, 64} (NotSupportedInGNULDForIFunc)
- R_AARCH64_PLT32 (UNSUPPORTED)

#### GOT-related data relocations

- GOTREL{32, 64} (UNSUPPORTED)
- GOTPCREL32 (UNSUPPORTED)

#### Control flow relocations

Resolves to PLT[IFuncSymbol].

- TSTBR14
- CONDBR19
- JUMP26
- CALL26

#### GOT-related instruction relocations

Resolves-to / uses GOTPLT[IFuncSymbol] if there is no direct reference to
IFuncSymbol; otherwise uses GOT[IFuncSymbol].

- R_AARCH64_ADR_GOT_PAGE
- R_AARCH64_LD{32,64}_GOT_LO12_NC (LD32 variant UNSUPPORTED)
- R_AARCH64_LD{32,64}_GOTPAGE_LO15 (LD32 variant UNSUPPORTED)
- R_AARCH64_GOT_LD_PREL19 (UNSUPPORTED)
- AUTH-ABI GOT relocations (UNSUPPORTED)
- R_AARCH64_MOVW_GOTOFF_G{0,1}{_NC} (UNSUPPORTED)

#### Absolute / PC-relative address-forming relocations

- R_AARCH64_ADR_PREL_LO21 (NotSupportedInGNULDForIFunc)
- R_AARCH64_ADR_PREL_PG_HI21{_NC}

Resolves to PLT[IFuncSymbol]. Sets HasDirectReference[IFuncSymbol] to true.

- R_AARCH64_MOVW_UABS_G{0,1,2,3}{_NC} (NotSupportedInGNULDForIFunc)
- R_AARCH64_SABS_G{0,1,2,3} (NotSupportedInGNULDForIFunc)
- MOVW_PREL_G{0, 1, 2, 3}{_NC} (UNSUPPORTED)

Resolves to IFuncSymbol.

[!IMPORTANT]
FIXME: We should perhaps resolve these relocations to the PLT[IFuncSymbol]
instead of the IFuncSymbol for ensuring pointer equality.

#### Absolute / PC-relative load/store relocations

- LD_PREL_LO19 (NotSupportedInGNULDForIFunc)
- LDST{8, 16, 32, 64, 128}_ABS_LO12_NC (NotSupportedInGNULDForIFunc)

These relocations does not make sense with IFunc symbols. Loading value
at a function address is an invalid behavior, and so is storing a value
at a function address.

[!IMPORTANT]
FIXME: It should be an error to use these relocations with IFunc symbols.

#### General computation relocation

- R_AARCH64_ADD_ABS_LO12_NC

Resolves to PLT[IFuncSymbol]. Set HasDirectReference[IFuncSymbol].