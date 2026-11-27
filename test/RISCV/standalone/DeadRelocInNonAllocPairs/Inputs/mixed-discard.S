        .section .text.live,"ax",@progbits
        .globl _start
_start:
        .word 0

        .section .text.gc_dead,"ax",@progbits
gc_dead:
        .word 0
        .section .discarded,"ax",@progbits
explicit_dead:
        .word 0

        .section .metadata,"",@note
        .balign 4
        .long 4, 4, 1
        .asciz "ELD"
        .reloc ., R_RISCV_ADD32, gc_dead
        .reloc ., R_RISCV_SUB32, explicit_dead
        .word 0
