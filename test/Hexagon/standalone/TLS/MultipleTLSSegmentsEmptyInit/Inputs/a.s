        .text
        .globl  main
        .type   main,@function
main:
        jumpr r31
        .size main, .-main

        .section .tbss,"awT",@nobits
        .globl b
        .p2align 2
b:
        .space 4
        .size b, 4

        .data
        .globl refs
refs:
        .word b@TPREL
