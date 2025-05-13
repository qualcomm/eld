	/* exact+relax needed while llvm/llvm-project#142702 is not yet fixed/landed */
	.option exact
	.option relax

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
	qc.e.jal	f
	qc.e.j	f

	.org	0x780
	qc.e.jal	f
	qc.e.j	f

	.org	0x880
	qc.e.jal	f
	qc.e.j	f

	.org	0xfff00
	qc.e.jal	f
	qc.e.j	f

	.org	0x100100
	qc.e.jal	f
	qc.e.j	f

	.size	main, .-main
