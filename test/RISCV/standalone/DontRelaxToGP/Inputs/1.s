	.text
	.align	1
	.globl	foo
	.type	foo, @function
foo:
	lui	a5,%hi(a)
	lw	a0,%lo(a)(a5)
	ret
	.size	foo, .-foo
	.globl	a
	.section	.data,"aw"
	.align	2
	.type	a, @object
	.size	a, 128
a:
	.word	1
	.section	.sdata,"aw"
	.align	2
	.type	b, @object
	.size	b, 128
b:
	.word	1
