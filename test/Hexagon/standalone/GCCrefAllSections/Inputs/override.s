	.text
	.file	"ld-temp.o"
	.section	.text.foo,"ax",@progbits,unique,1
	.globl	baz
	.falign
	.type	baz,@function
baz:
	{
          call bar
	}
.Lfunc_end2:
.Ltmp2:
	.size	baz, .Ltmp2-baz

	.section	.text.foo,"ax",@progbits,unique,2
	.globl	bar
	.falign
	.type	bar,@function
bar:
	{
          call foo
	}
.Lfunc_end1:
.Ltmp1:
	.size	bar, .Ltmp1-bar


	.section	.text.foo,"ax",@progbits,unique,3
	.globl	foo
	.falign
	.type	foo,@function
foo:
	{
          call baz
	}
.Lfunc_end0:
.Ltmp0:
	.size	foo, .Ltmp0-foo

	.ident	"QuIC LLVM Hexagon Clang version hexagon-clang-80-7069 (based on LLVM 3.9.0)"
	.section	".note.GNU-stack","",@progbits
