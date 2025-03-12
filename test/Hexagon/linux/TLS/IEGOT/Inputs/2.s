	.file	"2.c"
	.globl src1
	.section	.tdata,"awT",@progbits
	.p2align 2
	.type	src1, @object
	.size	src1, 4
src1:
	.word	10
	.globl src2
	.p2align 2
	.type	src2, @object
	.size	src2, 4
src2:
	.word	10
	.ident	"GCC: (Sourcery CodeBench Lite 5.1 2012.03-137) 4.6.1"
	.section	.note.GNU-stack,"",@progbits
