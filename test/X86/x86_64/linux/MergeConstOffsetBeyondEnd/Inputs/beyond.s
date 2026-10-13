	.section	.rodata.cst4,"aM",@progbits,4
.Lconst:
	.long	0x12345678
	.globl	way_past
way_past = .Lconst + 100
	.globl	midsym
midsym = .Lconst + 2
