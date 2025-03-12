	.text
	.globl	foo
	.type	foo,%function
foo:
        nop
.Lfunc_end0:
.Ltmp0:
	.size	foo, .Ltmp0-foo

	.type	bar,%object
	.section	.rodata.str1.1,"aMS",%progbits,4
	.globl	bar
	.p2align	2
bar:
	.word	10
	.size	bar, 4
        .word foo
