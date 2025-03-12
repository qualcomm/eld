	.text
	.file	"1.c"
	.type	foo,@object
	.section	.keep_cached,"aw",@progbits
	.globl	foo
	.align	4
foo:
	.word	100
	.size	foo, 4

	.type	data,@object
	.section	.pool.space,"aw",@nobits
	.globl	data
	.align	4
data:
	.word	0
	.size	data, 4
