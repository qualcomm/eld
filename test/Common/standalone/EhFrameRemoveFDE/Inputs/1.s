	.text
	.file	"1.c"
	.section	.text.bar,"ax"
	.hidden	bar                     // -- Begin function bar
	.globl	bar
	.balign 4
bar:                                    // @bar
	.cfi_startproc
.Lfunc_end0:
	.size	bar, .Lfunc_end0-bar
	.cfi_endproc
                                        // -- End function
	.section	.text.foo,"ax"
	.globl	foo                     // -- Begin function foo
	.balign 4
foo:                                    // @foo
	.cfi_startproc
.Lfunc_end1:
	.size	foo, .Lfunc_end1-foo
	.cfi_endproc
