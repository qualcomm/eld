	.text
	.file	"1.c"
	.globl	main
	.p2align	2
	.type	main,@function
main:                                   // @main
// BB#0:                                // %entry
	sub	sp, sp, #16             // =16
	mov	 w8, wzr
	str	wzr, [sp, #12]
	mov	 w0, w8
	add	sp, sp, #16             // =16
	ret
.Lfunc_end0:
	.size	main, .Lfunc_end0-main


	.ident	"Snapdragon LLVM ARM Compiler 4.0.0 (based on LLVM 4.0.0)"
