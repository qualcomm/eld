#include "asm-helper.h"

// Same payload as 1.c, but explicitly aligned to 4 bytes.
__asm__(".section .rodata.cst4,\"aM\"," SECTYPE ",4\n"
        ".p2align 2\n"
        ".globl c2\n"
        ".type c2, " OBJTYPE "\n"
        ".size c2, 4\n"
        "c2:\n"
        ".4byte 0x11223344\n");
