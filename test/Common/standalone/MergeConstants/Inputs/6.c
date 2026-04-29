#include "asm-helper.h"
// DistinctPayload input B: different bytes from 5.c, so it must stay separate.
__asm__(".section .rodata.cst4,\"aM\"," SECTYPE ",4\n"
        ".globl d2\n"
        ".type d2, " OBJTYPE "\n"
        ".size d2, 4\n"
        "d2:\n"
        ".4byte 0x55667788\n");
