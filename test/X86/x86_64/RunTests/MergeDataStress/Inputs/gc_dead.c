#include <stdint.h>
__asm__(".section .rodata.cst4.dead,\"aM\",@progbits,4\n"
        ".p2align 2\n"
        ".globl c_gc_dead\n.type c_gc_dead,@object\n.size c_gc_dead,4\n"
        "c_gc_dead:\n.long 0x13572468\n");
extern const uint32_t c_gc_dead;
__attribute__((section(".text.get_dead"))) const void *get_dead(void) {
  return &c_gc_dead;
}
