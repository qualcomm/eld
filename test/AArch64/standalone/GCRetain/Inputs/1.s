	.text
	.file	"1.c"
	.section	.text.foo,"axR",@progbits
	.globl	foo                             // -- Begin function foo
	.p2align	3
	.type	foo,@function
foo:                                    // @foo
// %bb.0:                               // %entry
	mov	w0, wzr
	ret
.Lfunc_end0:
	.size	foo, .Lfunc_end0-foo
                                        // -- End function
	.section	.text.main,"ax",@progbits
	.globl	main                            // -- Begin function main
	.p2align	3
	.type	main,@function
main:                                   // @main
// %bb.0:                               // %entry
	mov	w0, wzr
	ret
.Lfunc_end1:
	.size	main, .Lfunc_end1-main
                                        // -- End function
	.ident	"Snapdragon LLVM ARM Compiler 10.0.0"
	.section	".note.GNU-stack","",@progbits
	.addrsig
