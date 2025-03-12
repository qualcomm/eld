	.text
	.file	"1.c"
	.globl	baz
	.falign
	.type	baz,@function
baz:                                   // @baz
// BB#0:                                // %entry
	{
		call foo
		r0 = ##l
		allocframe(#0)
	}
	{
		call foo
		r0 = ##l1
	}
	{
		call foo
		r0 = ##l2
	}
	{
		r0 = #0
		dealloc_return
	}
.Lfunc_end0:
.Ltmp0:
	.size	baz, .Ltmp0-baz

	.type	l,@object          // @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
l:
	.string	"Hello"
	.size	l, 6

	.type	l1,@object        // @.str.1
l1:
	.string	"World"
	.size	l1, 6

	.type	l2,@object        // @.str.2
l2:
	.string	"ELD"
	.size	l2, 9

