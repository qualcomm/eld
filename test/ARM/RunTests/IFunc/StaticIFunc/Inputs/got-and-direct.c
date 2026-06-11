/* Provides BOTH a direct (R_ARM_ABS32) and a GOT-based (R_ARM_GOT_PREL)
 * reference to the IFUNC `foo`, in separate functions. Because the linker
 * sees both IFuncDirectRef and IFuncNeedsGOT for `foo`, it materializes a
 * dedicated regular GOT slot holding PLT[foo] so that the GOT-loaded pointer
 * equals the direct-reference pointer (pointer equality). The references live
 * in a different translation unit than `foo`'s definition so the assembler
 * cannot fold them to the resolver address. */

int foo();

int (*foo_gp)() = foo; /* R_ARM_ABS32: direct data reference */

int (*get_foo_direct())() { return foo_gp; }

int (*get_foo_via_got())() { return foo; } /* R_ARM_GOT_PREL: GOT reference */
