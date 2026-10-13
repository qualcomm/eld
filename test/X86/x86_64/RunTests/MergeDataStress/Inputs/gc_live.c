#include <stdint.h>
__asm__(".section .rodata.cst4.live,\"aM\",@progbits,4\n"
        ".p2align 2\n"
        ".globl c_gc_live\n.type c_gc_live,@object\n.size c_gc_live,4\n"
        "c_gc_live:\n.long 0xabcdef01\n");
extern const uint32_t c_gc_live;
__attribute__((section(".text.get_live"))) const void *get_live(void) {
  return &c_gc_live;
}
