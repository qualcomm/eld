        .text
        .globl  patch
        .type   patch,@function
patch:
        lui     a0, %hi(xxx_f)
        addi    a0, a0, %lo(xxx_f)
        lui     a0, %hi(xxx_g)
        addi    a0, a0, %lo(xxx_g)
        lui     a0, %hi(xxx_h)
        addi    a0, a0, %lo(xxx_h)
.Lfunc_end0:
        .size   patch, .-patch
