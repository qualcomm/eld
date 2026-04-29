#include "asm-helper.h"
// Same payload as 7.c/8.c with stricter 16-byte alignment; final canonical
// pick.
__asm__(".section .rodata.cst4,\"aM\"," SECTYPE ",4\n"
        ".p2align 4\n"
        ".globl t3\n"
        ".type t3, " OBJTYPE "\n"
        ".size t3, 4\n"
        "t3:\n"
        ".4byte 0x99aabbcc\n");
