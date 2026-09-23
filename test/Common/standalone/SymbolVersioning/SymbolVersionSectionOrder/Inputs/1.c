// Large, highly-aligned .bss object plus a small .data object. The high
// alignment forces .bss to a large virtual address, so if the read-only
// .gnu.version* sections were ranked after .data/.bss they would strand a
// read-only LOAD segment past the last writable segment.
float Input[4] __attribute__((aligned(65536)));
int initialized = 7;

int foo() { return 1; }
int bar() { return 3; }
int baz() { return 5; }
