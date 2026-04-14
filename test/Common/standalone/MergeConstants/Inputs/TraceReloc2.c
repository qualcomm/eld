#include "asm-helper.h"
// Companion fragment for TraceReloc1.c; forces relocation retarget trace
// updates.
__asm__(".section .rodata.cst4,\"aM\"," SECTYPE ",4\n"
        ".p2align 4\n"
        "r2:\n"
        ".4byte 0x11223344\n"
        ".data\n"
        "p2:\n"
        ".4byte .rodata.cst4+0\n");
