        .section .text.live,"ax",@progbits
        .globl _start
_start:
        .byte 0

#ifdef EXPLICIT_DISCARD
        .section .discarded,"ax",@progbits
#else
        .section .text.dead,"ax",@progbits
#endif
        .globl dead
dead:
        .byte 0

        .section .debug_pair,"",@progbits
        .reloc ., R_RISCV_SET_ULEB128, _start+5
        .reloc ., R_RISCV_SUB_ULEB128, dead+3
        .byte 0x80, 0
