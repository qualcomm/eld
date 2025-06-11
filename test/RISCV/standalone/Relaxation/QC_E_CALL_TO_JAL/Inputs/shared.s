	.text
	.global f
f:
	/* exact+relax needed while llvm/llvm-project#142702 is not yet fixed/landed */
	.option exact
	.option relax

	qc.e.jal	f
	qc.e.j	f
