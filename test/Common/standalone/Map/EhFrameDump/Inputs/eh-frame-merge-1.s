	.text
	.globl	foo
	.type	foo, @function
foo:
	.cfi_startproc
	nop
	.cfi_endproc
	.size	foo, .-foo
