__asm__(".section .orphan.cst4,\"aM\",%progbits,4\n"
        ".globl orphan1\n"
        ".type orphan1, %object\n"
        ".size orphan1, 4\n"
        "orphan1:\n"
        ".4byte 0x11223344\n");
