/* Both REX_GOTPCRELX (address-taken) and PLT32 (call) for the same symbol.
   The linker should produce one .got slot (GLOB_DAT) and a .plt.got stub,
   not two slots (GLOB_DAT + JUMP_SLOT). */
extern void foo(void);

void (*foo_ptr)(void);

void use_foo(void) {
  foo_ptr = foo; /* R_X86_64_REX_GOTPCRELX → MayNeedGOT */
  foo();         /* R_X86_64_PLT32         → MayNeedPLT */
  foo_ptr();
}
