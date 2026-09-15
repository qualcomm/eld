	.section	.rodata.str1.1,"aMS",@progbits,1
.Lstr:
	.asciz	"hello"
	.globl	way_past
way_past = .Lstr + 100
	.globl	midsym
midsym = .Lstr + 2
