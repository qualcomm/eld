        .text
        .globl  f
        .type   f,@function
f:
        .globl  __llvm_patchable_f
__llvm_patchable_f:
        nop
        .size   f, .-f

        .globl  main
        .type   main,@function
main:
        lui     a0, %hi(p)
        addiw   a0, a0, %lo(p)
        .size   main, .-main

        .type   p,@object
        .section        .data,"aw",@progbits
        .globl  p
p:
        .quad   f
        .size   p, 8
