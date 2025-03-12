	.section	.text.foo,"ax",@progbits
	.globl	foo
	.p2align 2
	.type	foo,@function
foo:                                    // @foo
// BB#0:                                // %entry
	{
		r0 = #0
		jumpr r31
	}

	.section	.text.coo,"ax",@progbits
	.globl	coo
	.p2align 2
	.type	coo,@function
coo:                                    // @coo
// BB#0:                                // %entry
	{
		r0 = #0
		jumpr r31
	}

	.section	.text.moo,"ax",@progbits
	.globl	moo
	.p2align 2
	.type	moo,@function
moo:                                    // @moo
// BB#0:                                // %entry
	{
		r0 = #0
		jumpr r31
	}
