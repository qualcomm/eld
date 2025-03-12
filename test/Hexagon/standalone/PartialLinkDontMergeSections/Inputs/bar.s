	.text
	.globl	crap
	.falign
	.type	crap,@function
crap:
        nop
.Lfunc_end0:
.Ltmp0:
	.size	crap, .Ltmp0-crap

	.type	anotherbar,@object
	.section	.rodata.str1.1,"aMS",@progbits,1
	.globl	anotherbar
	.p2align	2
anotherbar:
	.word	20
	.size	anotherbar, 4
        .word crap
