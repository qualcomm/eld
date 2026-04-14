#include "asm-helper.h"
// Second identical mergeable fragment used to trigger merge tracing output.
__asm__(".section .rodata.cst4,\"aM\"," SECTYPE ",4\n"
        ".p2align 2\n"
        "m2:\n"
        ".4byte 0x11223344\n");
