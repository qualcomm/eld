#include "asm-helper.h"
// DistinctPayload input A: mergeable constant with unique data pattern.
__asm__(".section .rodata.cst4,\"aM\"," SECTYPE ",4\n"
        ".globl d1\n"
        ".type d1, " OBJTYPE "\n"
        ".size d1, 4\n"
        "d1:\n"
        ".4byte 0x11223344\n");
