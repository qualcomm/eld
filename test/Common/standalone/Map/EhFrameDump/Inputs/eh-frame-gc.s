	.section .text.main,"ax",@progbits
	.globl	main
	.type	main, @function
main:
	.cfi_startproc
	call	foo
	nop
	.cfi_endproc
	.size	main, .-main

	.section .text.foo,"ax",@progbits
	.globl	foo
	.type	foo, @function
foo:
	.cfi_startproc
	nop
	.cfi_endproc
	.size	foo, .-foo

	.section .text.bar,"ax",@progbits
	.globl	bar
	.type	bar, @function
bar:
	.cfi_startproc
	nop
	.cfi_endproc
	.size	bar, .-bar
