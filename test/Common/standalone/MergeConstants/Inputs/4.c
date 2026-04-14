#include "asm-helper.h"
// Same payload pattern as 3.c but in entsize-8 section; must not cross-merge.
__asm__(".section .rodata.cst8,\"aM\"," SECTYPE ",8\n"
        ".globl e8a\n"
        ".type e8a, " OBJTYPE "\n"
        ".size e8a, 8\n"
        "e8a:\n"
        ".8byte 0x11223344\n"
        ".globl e8b\n"
        ".type e8b, " OBJTYPE "\n"
        ".size e8b, 8\n"
        "e8b:\n"
        ".8byte 0x11223344\n");
