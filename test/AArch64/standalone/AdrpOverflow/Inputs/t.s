	.text
	.globl	foo
	.p2align	2
	.type	foo,@function
foo:
	adrp	x0, bar
	ret
.Lfunc_end0:
	.size	foo, .Lfunc_end0-foo
