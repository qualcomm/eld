#include "asm-helper.h"
// Provides _start and one mergeable constant fragment used as the merge target.
__asm__(".text\n"
        ".globl _start\n"
        "_start:\n"
        "nop\n"
        ".section .rodata.cst4,\"aM\"," SECTYPE ",4\n"
        ".p2align 2\n"
        "m1:\n"
        ".4byte 0x11223344\n");
