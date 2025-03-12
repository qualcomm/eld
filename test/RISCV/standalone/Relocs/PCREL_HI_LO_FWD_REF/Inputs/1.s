	.section .data.num
num:
        .word   1
	.section .data.val
val:
        .word   2
	
        .section .text.test
	.globl	test
	.type	test,@function
test:
.Lpcrel_hi0:
	auipc	a0, %pcrel_hi(num)
	lw	s0, %pcrel_lo(.Lpcrel_hi0)(a0)
	j	.LBB0_2
.LBB0_1:
	sw	zero, %pcrel_lo(.Lpcrel_hi5)(a0)
.LBB0_2:
	call	foo@plt
.Lpcrel_hi5:
	auipc	a0, %pcrel_hi(val)
	lw	a1, %pcrel_lo(.Lpcrel_hi5)(a0)
	bnez	a1, .LBB0_1
	j	.LBB0_2

        .section .text.foo
	.globl	foo
	.type	foo,@function
foo:
        nop
