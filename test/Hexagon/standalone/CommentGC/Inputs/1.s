	.text
	.file	"1.c"
	.globl	main
	.falign
	.type	main,@function
main:
	{
		allocframe(#8)
	}
	{
		r2 = add(r29, #4)
	}
	{
		memw(r2+#0)=#0
	}
	{
		r0 = #0
	}
	{
		dealloc_return
	}
.Ltmp0:
	.size	main, .Ltmp0-main


	.ident	"QuIC LLVM Hexagon Clang version %clang-72-5565"
