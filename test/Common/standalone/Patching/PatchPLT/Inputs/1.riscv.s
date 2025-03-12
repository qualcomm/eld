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
        call    f
        .size   main, .-main
