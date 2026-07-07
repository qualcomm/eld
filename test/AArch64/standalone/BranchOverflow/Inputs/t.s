	.text
	.globl	foo
	.p2align	2
	.type	foo,@function
foo:
	bl	bar
	ret
.Lfunc_end0:
	.size	foo, .Lfunc_end0-foo

	.globl	baz
	.p2align	2
	.type	baz,@function
baz:
	b	bar
.Lfunc_end1:
	.size	baz, .Lfunc_end1-baz
