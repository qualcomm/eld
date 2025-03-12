	.text
	.globl	main
	.p2align	2
	.type	main,@function
main:                                   // @main
// BB#0:                                // %entry
	sub	sp, sp, #32             // =32
	stp	x29, x30, [sp, #16]     // 8-byte Folded Spill
	add	x29, sp, #16            // =16
	stur	wzr, [x29, #-4]
	bl	bar
	ldp	x29, x30, [sp, #16]     // 8-byte Folded Reload
	add	sp, sp, #32             // =32
	ret
