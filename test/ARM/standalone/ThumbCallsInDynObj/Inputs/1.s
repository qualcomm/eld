        .text
        .cpu    cortex-a8
        .globl  main                    @ -- Begin function main
        .p2align        1
        .type   main,%function
        .code   16                      @ @main
        .thumb_func
main:
        b.w     bar
.Lfunc_end0:
        .size   main, .Lfunc_end0-main
