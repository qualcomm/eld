	.text
	.file	"main.c"
	.section	.text.main,"ax",@progbits
	.globl	main
        .p2align 2
	.type	main,@function
main:                                   // @main
	bl	FarFunctionA
.Lfunc_end0:
.Ltmp0:
	.size	main, .Ltmp0-main
