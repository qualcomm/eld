	.text
	.globl	foo
	.p2align	2
	.type	foo,@function
foo:                                    // @foo
// BB#0:                                // %entry
	b.eq	bar
	ret
.Lfunc_end0:
.Ltmp0:
	.size	foo, .Ltmp0-foo
