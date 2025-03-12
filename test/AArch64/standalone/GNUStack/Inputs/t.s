	.text
	.file	"t.bc"
	.globl	main
	.align	2
	.type	main,@function
main:                                   // @main
// BB#0:                                // %entry
	sub	sp, sp, #16             // =16
	mov	 w8, wzr
	str	wzr, [sp, #12]
	mov	 w0, w8
	add	sp, sp, #16             // =16
	ret
.Ltmp1:
	.size	main, .Ltmp1-main

