extern const float mc1;

// Emit a mergeable 4-byte constant from C so the PIC test still exercises
// SHF_MERGE constant deduplication and trace output.
__asm__(".section .rodata.cst4,\"aM\",@progbits,4\n"
        ".p2align 2\n"
        ".globl mc1\n"
        ".type mc1,@object\n"
        "mc1:\n"
        ".long 0x40600000\n"
        ".size mc1, 4\n"
        ".text\n");

const float *fp1(void) { return &mc1; }
float scale1(void) { return *fp1(); }
