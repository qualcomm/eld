#include "asm-helper.h"
// Baseline mergeable constant: 4-byte payload, no explicit alignment directive.
__asm__(".section .rodata.cst4,\"aM\"," SECTYPE ",4\n"
        ".globl c1\n"
        ".type c1, " OBJTYPE "\n"
        ".size c1, 4\n"
        "c1:\n"
        ".4byte 0x11223344\n");
