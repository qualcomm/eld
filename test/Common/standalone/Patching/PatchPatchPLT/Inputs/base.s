        .text
        .globl  xxx_f
        .type   xxx_f,@function
xxx_f:
        .globl  __llvm_patchable_xxx_f
__llvm_patchable_xxx_f:
        nop
        .size   xxx_f, .-xxx_f

        .globl  xxx_g
        .type   xxx_g,@function
xxx_g:
        .globl  __llvm_patchable_xxx_g
__llvm_patchable_xxx_g:
        nop
        .size   xxx_g, .-xxx_g

        .globl  xxx_h
        .p2align        1
        .type   xxx_h,@function
xxx_h:
        nop
        .size   xxx_h, .-xxx_h

        .globl  xxx_k
        .type   xxx_k,@function
xxx_k:
        nop
        .size   xxx_k, .-xxx_k

        .globl  main
        .type   main,@function
main:
        call    xxx_f
        call    xxx_g
        call    xxx_h
        call    xxx_k
        .size   main, .-main
