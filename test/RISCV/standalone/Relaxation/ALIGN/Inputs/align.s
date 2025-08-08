	.text
	.align	1
	.globl	main
	.type	main, @function
main:
	call	norelax
	.align  4
	call	main

.equ norelax, 0x40000000
