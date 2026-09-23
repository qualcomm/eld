        .text
        .globl  main
        .type   main,@function
main:
        jumpr r31
        .size main, .-main

        .section .tdata,"awT",@progbits
        .globl a
        .p2align 2
a:
        .word 1
        .size a, 4

        .section .tbss,"awT",@nobits
        .globl b
        .p2align 6
b:
        .space 4
        .size b, 4

        .data
        .globl refs
refs:
        .word a@TPREL
        .word b@TPREL
