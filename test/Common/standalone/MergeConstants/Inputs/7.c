#include "asm-helper.h"
// Alignment promotion chain start: payload shared by t1/t2/t3 with weak
// alignment.
__asm__(".section .rodata.cst4,\"aM\"," SECTYPE ",4\n"
        ".globl t1\n"
        ".type t1, " OBJTYPE "\n"
        ".size t1, 4\n"
        "t1:\n"
        ".4byte 0x99aabbcc\n");
