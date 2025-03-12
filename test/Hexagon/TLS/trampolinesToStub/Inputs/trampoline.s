	.text
	.file	"b.bc"
	.globl	foo
	.falign
	.type	foo,@function
foo:                                    // @foo
// BB#0:                                // %entry
	{
		allocframe(#8)
	}
	{
		r2 = add(pc, ##_GLOBAL_OFFSET_TABLE_@PCREL)
	}
	{
		r3 = ##src1@GDGOT
	}
	{
		r0 = add(r3, r2)
	}
	{
		call src1@GDPLT
	}
	{
		r2 = memw(r0 + #0)
	}
	{
		memw(r30+#-4) = r2
	}
	{
		r0 = memw(r30 + #-4)
	}
	{
		dealloc_return
	}
.Ltmp0:
	.size	foo, .Ltmp0-foo

	.globl	main
	.falign
	.type	main,@function
main:                                   // @main
// BB#0:                                // %entry
	{
		allocframe(#8)
	}
	{
		r2 = #0
	}
	{
		memw(r30+#-4) = r2
	}
	{
		call foo
	}
	{
		dealloc_return
	}
.Ltmp1:
	.size	main, .Ltmp1-main

	.type	src1,@object            // @src1
	.section	.tdata,"awT",@progbits
	.globl	src1
	.align	4
src1:
	.word	1                       // 0x1
	.size	src1, 4


	.ident	"QuIC LLVM Hexagon Linux Clang version hexagon-linux-toolchain-80-103 (based on LLVM 3.7.0)"
	.section	".note.GNU-stack","",@progbits
