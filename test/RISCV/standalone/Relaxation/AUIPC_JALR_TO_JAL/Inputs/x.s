	.text
	.align	1
	.type	f, @function
f:
	ret
	.size	f, .-f

	.align	1
	.globl	main
	.type	main, @function
main:
	call	f

	.org	0x780
	call	f

	.org	0x880
	call	f

	.org	0xfff00
	call	f

	.org	0x100100
	call	f

	.size	main, .-main
