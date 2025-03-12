#----------NoRelaxGP.s----------------- Executable------------------#


# BEGIN_COMMENT
# Test that --no-relax-gp disables gp relaxation
# END_COMMENT

#RUN: %clang %clangopts -c %s -o %t.o
#RUN: %link %linkopts --no-relax-gp %t.o -T %p/Inputs/script.t -o %t.out --verbose 2>&1 | %filecheck %s
#RUN: %nm %t.out | %filecheck %s --check-prefix=GP
#RUN: %link %linkopts  %t.o -T %p/Inputs/script.t -o %t1.out --verbose -M 2>&1 | %filecheck %s -check-prefix=RELAXBYTES
#RUN: %nm %t.out | %filecheck %s --check-prefix=GP
#CHECK-NOT: RISCV_PC_GP : Deleting
#CHECK-NOT: RISCV_LUI_GP : Deleting
#RELAXBYTES: RelaxationBytesDeleted : 4
#GP: 00002000 D __global_pointer$

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
