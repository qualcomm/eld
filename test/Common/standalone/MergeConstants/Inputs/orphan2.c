__asm__(".section .orphan.cst4,\"aM\",%progbits,4\n"
        ".globl orphan2\n"
        ".type orphan2, %object\n"
        ".size orphan2, 4\n"
        "orphan2:\n"
        ".4byte 0x11223344\n");
