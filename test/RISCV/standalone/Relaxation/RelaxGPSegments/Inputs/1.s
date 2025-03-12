        .text
        .globl  foo
        .type   foo, @function
foo:
        nop
        nop
        nop
        nop
.Lpcrel_hi6:
# RISCV_PC_GP
        auipc   a5,%pcrel_hi(a)
        lw      a0,%pcrel_lo(.Lpcrel_hi6)(a5)
.Lpcrel_hi5:
# RISCV_PC_GP
        auipc   a5,%pcrel_hi(a)
        lw      a0,%pcrel_lo(.Lpcrel_hi5)(a5)
.Lpcrel_hi4:
# RISCV_PC_GP
        auipc   a5,%pcrel_hi(a)
        lw      a0,%pcrel_lo(.Lpcrel_hi4)(a5)

        .globl  bar
        .type   bar, @function
# RISCV_LUI_GP
bar:
        lui     a5,%hi(b)
        lw      a0,%lo(b)(a5)
        ret
        .size   bar, .-bar

# dummy globals
        .globl  a
        .section        .a_sec,"aw"
        .type   a,@object
        .size   a,4
a:
        .word   1
        .section        .sdata,"aw"
        .type   b,@object
        .size   b,4
b:
        .word   1
