	.text
	.file	"1.c"
	.globl	main
	.falign
	.type	main,@function
main:                                   // @main
// BB#0:                                // %entry
	{
		call foo
		memd(r29+#-16)=r17:16
		allocframe(#8)
	}
	{
		call bar
		r16=r0
	}
	{
		r0=add(r0,r16)
		r17:16=memd(r29+#0)
		dealloc_return
	}
.Lfunc_end0:
	.size	main, .Lfunc_end0-main

	.weak	foo
        .type foo, @function
	.weak	bar
        .type bar, @function

	.ident	"QuIC LLVM Hexagon Clang version hexagon-clang-82-1737 (based on LLVM 4.0.0)"
	.section	".note.GNU-stack","",@progbits
