	.text
	.file	"g.i"
	.section	.text.QURTK_fatal_hook,"axG",@progbits,QURTK_fatal_hook,comdat
	.weak	QURTK_fatal_hook
	.falign
	.type	QURTK_fatal_hook,@function
QURTK_fatal_hook:
	{
		allocframe(#0)
	}
	{
		call blah
	}
	{
		dealloc_return
	}
.Ltmp0:
	.size	QURTK_fatal_hook, .Ltmp0-QURTK_fatal_hook

	.text
	.globl	blah
	.falign
	.type	blah,@function
blah:
	{
		allocframe(#0)
	}
	{
		r0 = #0
	}
	{
		dealloc_return
	}
.Ltmp1:
	.size	blah, .Ltmp1-blah


	.ident	"QuIC LLVM Hexagon Clang version %clang-73-1293"
