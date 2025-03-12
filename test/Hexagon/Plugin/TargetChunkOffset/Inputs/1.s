	.text
	.file	"1.c"
	.section	.text.bar,"ax",@progbits
	.globl	bar                             // -- Begin function bar
	.falign
	.type	bar,@function
bar:                                    // @bar
// %bb.0:                               // %entry
	{
		allocframe(r29,#8):raw
	}
	{
		memw(r30+#-4) = r0
	}
	{
		r0 = #0
	}
	{
		r31:30 = dealloc_return(r30):raw
	}
.Lfunc_end0:
	.size	bar, .Lfunc_end0-bar
                                        // -- End function
	.section	.text.foo,"ax",@progbits
	.globl	foo                             // -- Begin function foo
	.falign
	.type	foo,@function
foo:                                    // @foo
// %bb.0:                               // %entry
	{
		allocframe(r29,#0):raw
	}
	{
		r0 = ##.L.str
	}
	{
		call bar
	}
	{
		r0 = ##.L.str.1
	}
	{
		call bar
	}
	{
		r0 = #0
	}
	{
		r31:30 = dealloc_return(r30):raw
	}
.Lfunc_end1:
	.size	foo, .Lfunc_end1-foo
                                        // -- End function
	.type	.L.str,@object                  // @.str
	.section	.rodata.val,"S",@progbits
.L.str:
	.string	"hello"
	.size	.L.str, 6

	.type	.L.str.1,@object                // @.str.1
.L.str.1:
	.string	"world"
	.size	.L.str.1, 6

	.ident	"QuIC LLVM Hexagon Clang version 8.6-alpha3 Engineering Release: hexagon-clang-86-5610"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym bar
