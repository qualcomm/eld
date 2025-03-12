	.section	.init,"ax",@progbits
	.globl	caz
	.p2align 4
	.type	caz,@function
caz:                                    // @caz
// BB#0:                                // %entry
        //call #moo
	{
		r0 = #0
		jumpr r31
	}
