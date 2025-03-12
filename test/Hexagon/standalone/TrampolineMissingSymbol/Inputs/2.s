	.section	.init,"ax",@progbits
	.globl	baz
	.p2align 10
	.type	baz,@function
baz:                                    // @baz
// BB#0:                                // %entry
        call #coo
	{
		r0 = #0
		jumpr r31
	}
