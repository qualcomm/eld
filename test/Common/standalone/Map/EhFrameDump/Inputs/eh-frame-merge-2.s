	.text
	.globl	bar
	.type	bar, @function
bar:
	.cfi_startproc
	nop
	.cfi_endproc
	.size	bar, .-bar
