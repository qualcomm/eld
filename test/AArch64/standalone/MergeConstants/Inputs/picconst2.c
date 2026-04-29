extern const float mc2;

// Twin definition of mc1's bytes in a separate TU so the linker can merge it.
__asm__(".section .rodata.cst4,\"aM\",@progbits,4\n"
        ".p2align 2\n"
        ".globl mc2\n"
        ".type mc2,@object\n"
        "mc2:\n"
        ".long 0x40600000\n"
        ".size mc2, 4\n"
        ".text\n");

const float *fp2(void) { return &mc2; }
float scale2(void) { return *fp2(); }
