        .text
        .globl  foo
        .type   foo,@function
foo:
.Lpcrel_hi0:
        auipc   a0, %pcrel_hi(.rodata.str+1)
        addi    a0, a0, %pcrel_lo(.Lpcrel_hi0)
        lw      a2, 0(a0)
.Lfunc_end0:
        .size   foo, .Lfunc_end0-foo

        .section        .rodata.str,"aMS",@progbits,1
        .asciz  "constant string literal"
