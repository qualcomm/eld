        .text
        .globl _start
_start:
        addi    a0, a0, %pcrel_lo(.L2)
        call    _start
.L2:
        auipc   a0, %pcrel_hi(foo)
        .section .sdata,"aw"
        .word 0x0
        .globl foo
foo:
        .word 0x1
