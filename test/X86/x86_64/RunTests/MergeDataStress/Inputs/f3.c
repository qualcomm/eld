#include <stdint.h>
__asm__(".section .rodata.cst4,\"aM\",@progbits,4\n"
        ".p2align 2\n"
        ".globl m3\n.type m3,@object\n.size m3,4\n"
        "m3:\n.long 0x11223344\n"
        ".globl x3\n.type x3,@object\n.size x3,4\n"
        "x3:\n.long 0xdeadbeef\n");
