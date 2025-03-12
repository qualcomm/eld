        .section .text.foo,"axG",%progbits,foo,comdat
        .globl foo
        .p2align 2
        .type foo,%function
foo:
        nop
.Lfunc_end0:
        .size   foo, .Lfunc_end0-foo
        .section .debug_loc
        .word .text.foo
