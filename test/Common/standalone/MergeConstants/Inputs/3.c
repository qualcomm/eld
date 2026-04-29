#include "asm-helper.h"
// Two identical constants in entsize-4 merge section; should fold to one value.
__asm__(".section .rodata.cst4,\"aM\"," SECTYPE ",4\n"
        ".globl e4a\n"
        ".type e4a, " OBJTYPE "\n"
        ".size e4a, 4\n"
        "e4a:\n"
        ".4byte 0x11223344\n"
        ".globl e4b\n"
        ".type e4b, " OBJTYPE "\n"
        ".size e4b, 4\n"
        "e4b:\n"
        ".4byte 0x11223344\n");
