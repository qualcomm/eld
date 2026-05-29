// Distant callee function placed in a far section
// This will require a trampoline for ARM BL instruction

__attribute__((section(".text.distant"))) int distant_callee(void) {
  return 42;
}
