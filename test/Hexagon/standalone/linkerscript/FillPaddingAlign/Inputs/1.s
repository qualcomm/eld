	.text
	.file	"1.c"
	.section	.text.foo,"ax",@progbits
	.globl	foo
	.falign
	.type	foo,@function
foo:                                    // @foo
// BB#0:                                // %entry
	{
		allocframe(#0)
	}
	{
		r0 = ##.L.str
	}
	{
		dealloc_return
	}
.Lfunc_end0:
.Ltmp0:
	.size	foo, .Ltmp0-foo

	.section	.text.bar,"ax",@progbits
        .p2align 8
	.globl	bar
	.falign
	.type	bar,@function
bar:                                    // @bar
// BB#0:                                // %entry
	{
		allocframe(#0)
	}
	{
		r0 = #0
	}
	{
		dealloc_return
	}
.Lfunc_end1:
.Ltmp1:
	.size	bar, .Ltmp1-bar

	.type	.L.str,@object          // @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.string	"gi"
	.size	.L.str, 3


	.ident	"QuIC LLVM Hexagon Clang version hexagon-clang-80-9572 (based on LLVM 3.9.0)"
	.section	".note.GNU-stack","",@progbits
