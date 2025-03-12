	.text
	.file	"1.c"
	.type	b,@object               // @b
	.section	.bss.b,"aw",@nobits
	.globl	b
	.p2align	2
b:
	.word	0                       // 0x0
	.size	b, 4

	.type	a,@object               // @a
	.section	.data.a,"aw",@progbits
	.globl	a
	.p2align	2
a:
	.word	10                      // 0xa
	.size	a, 4

	.type	c,@object               // @c
	.section	.bss.c,"aw",@nobits
	.globl	c
	.p2align	2
c:
	.word	0                       // 0x0
	.size	c, 4


	.ident	"QuIC LLVM Hexagon Clang version hexagon-clang-82-1674 (based on LLVM 4.0.0)"
	.section	".note.GNU-stack","",@progbits
