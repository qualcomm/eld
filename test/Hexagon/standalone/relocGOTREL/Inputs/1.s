	.text
	.file	"1.c"
	.globl	foo
	.falign
	.type	foo,@function
foo:                                    // @foo
// BB#0:                                // %entry
	{
		allocframe(#0)
	}
	{
		r2 = memw(r2 + ##a@GOTREL)
	}
	{
		dealloc_return
	}
.Lfunc_end0:
.Ltmp0:
	.size	foo, .Ltmp0-foo

	.type	a,@object               // @a
	.data
	.globl	a
	.align	4
a:
	.word	10                      // 0xa
	.size	a, 4
