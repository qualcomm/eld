        .section .text.live,"ax",@progbits
        .globl _start
_start:
        .byte 0

        .section .text.dead,"ax",@progbits
        .globl dead
dead:
        .byte 0

        .section .debug_pairs_uleb,"",@progbits
        .reloc ., R_RISCV_32, _start+0x70
        .reloc ., R_RISCV_SET_ULEB128, dead+5
        .reloc ., R_RISCV_SUB_ULEB128, _start-32
        .word 0x80
