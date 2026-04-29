#include "asm-helper.h"
// Same payload as 7.c with 4-byte alignment; candidate should replace weaker
// one.
__asm__(".section .rodata.cst4,\"aM\"," SECTYPE ",4\n"
        ".p2align 2\n"
        ".globl t2\n"
        ".type t2, " OBJTYPE "\n"
        ".size t2, 4\n"
        "t2:\n"
        ".4byte 0x99aabbcc\n");
