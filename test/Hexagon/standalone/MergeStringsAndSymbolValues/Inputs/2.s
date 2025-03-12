	.text
	.file	"1.c"
	.globl	bar
	.falign
	.type	bar,@function
bar:                                   // @bar
// BB#0:                                // %entry
	{
		call foo
		r0 = ##m
		allocframe(#0)
	}
	{
		call foo
		r0 = ##m1
	}
	{
		call foo
		r0 = ##m2
	}
	{
		r0 = #0
		dealloc_return
	}
.Lfunc_end0:
.Ltmp0:
	.size	bar, .Ltmp0-bar

	.type	m,@object          // @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
m:
	.string	"Hello"
	.size	m, 6

	.type	m1,@object        // @.str.1
m1:
	.string	"World"
	.size	m1, 6

	.type	m2,@object        // @.str.2
m2:
	.string	"ELD"
	.size	m2, 9
