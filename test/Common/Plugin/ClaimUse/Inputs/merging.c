// Section-relative references to duplicate strings/constants, including an
// interior offset. Use explicit section symbols, not compiler-generated labels.
#if __SIZEOF_POINTER__ == 8
#define POINTER ".quad "
#else
#define POINTER ".4byte "
#endif

#ifdef NO_MERGE
#define STRING_SECTION ".section .rodata.claim.str,\"a\",%progbits\n"
#else
#define STRING_SECTION ".section .rodata.claim.str,\"aMS\",%progbits,1\n"
#endif

#ifndef SECOND
__asm__(".text\n.globl _start\n_start:\nnop\n");
#define PREFIX "prefix"
#else
#define PREFIX "other!"
#endif

__asm__(STRING_SECTION ".asciz \"" PREFIX "\"\n"
                       ".asciz \"shared\"\n"
                       ".asciz \"shared\"\n"
                       ".section .rodata.claim.cst4,\"aM\",%progbits,4\n"
                       ".balign 4\n"
                       ".4byte 0x11111111\n"
                       ".4byte 0x11223344\n"
                       ".4byte 0x11223344\n"
                       ".section .data.claimed,\"aw\",%progbits\n" POINTER
                       ".rodata.claim.str+16\n" POINTER ".rodata.claim.cst4+9\n"
                       ".section .data.control,\"aw\",%progbits\n" POINTER
                       ".rodata.claim.str+16\n" POINTER
                       ".rodata.claim.cst4+5\n");
