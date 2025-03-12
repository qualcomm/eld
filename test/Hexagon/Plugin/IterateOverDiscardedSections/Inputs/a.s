	.text
	.file	"1.c"
	.section	.text.foo,"ax",@progbits
	.globl	foo                     // -- Begin function foo
	.falign
	.type	foo,@function
foo:                                    // @foo
// BB#0:                                // %entry
	{
		r0 = #0
		jumpr r31
	}
.Lfunc_end0:
	.size	foo, .Lfunc_end0-foo
                                        // -- End function
	.section	.text.bar,"ax",@progbits
	.globl	bar                     // -- Begin function bar
	.falign
	.type	bar,@function
bar:                                    // @bar
// BB#0:                                // %entry
	{
		r0 = #0
		jumpr r31
	}
.Lfunc_end1:
	.size	bar, .Lfunc_end1-bar
                                        // -- End function
	.section	.text.baz,"ax",@progbits
	.globl	baz                     // -- Begin function baz
	.falign
	.type	baz,@function
baz:                                    // @baz
// BB#0:                                // %entry
	{
		r0 = #0
		jumpr r31
	}
.Lfunc_end2:
	.size	baz, .Lfunc_end2-baz
                                        // -- End function
        .section ".note.llvm.callgraph", "a", @progbits
        nop
	.ident	"QuIC LLVM Hexagon Clang version hexagon-clang-83-2172 (based on LLVM 6.0.0)"
	.section	".note.GNU-stack","",@progbits
