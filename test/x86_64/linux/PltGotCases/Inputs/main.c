extern void foo_both(void);
extern void foo_call_only(void);
extern void foo_addr_only(void);

/* Case 4: defined in same TU → non-preemptible, no GOT/PLT needed */
__attribute__((visibility("hidden"))) void foo_hidden(void) {}

/* Case 1: both address-taken AND called → .plt.got optimization */
void (*ptr_both)(void);
void use_both(void) {
  ptr_both = foo_both; /* R_X86_64_REX_GOTPCRELX */
  foo_both();          /* R_X86_64_PLT32 */
}

/* Case 2: called only → normal .plt + .gotplt + JUMP_SLOT */
void use_call_only(void) { foo_call_only(); /* R_X86_64_PLT32 only */ }

/* Case 3: address-taken only → normal .got + GLOB_DAT, no PLT */
void (*ptr_addr_only)(void);
void use_addr_only(void) {
  ptr_addr_only = foo_addr_only; /* R_X86_64_REX_GOTPCRELX only */
}

/* Case 4: hidden symbol called → non-preemptible, no GOT/PLT */
void use_hidden(void) {
  foo_hidden(); /* R_X86_64_PLT32, but non-preemptible */
}
