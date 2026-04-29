#include "asm-helper.h"
// Defines _start, a mergeable constant, and a .data relocation to that section.
__asm__(".text\n"
        ".globl _start\n"
        "_start:\n"
        "nop\n"
        ".section .rodata.cst4,\"aM\"," SECTYPE ",4\n"
        "r1:\n"
        ".4byte 0x11223344\n"
        ".data\n"
        "p1:\n"
        ".4byte .rodata.cst4+0\n");
