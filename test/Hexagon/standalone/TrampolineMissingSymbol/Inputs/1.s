	.section	.init,"ax",@progbits
	.globl	bar
        .p2align 4
	.type	bar,@function
bar:                                    // @bar
// BB#0:                                // %entry
        //call #foo
	{
		r0 = #0
		jumpr r31
	}
