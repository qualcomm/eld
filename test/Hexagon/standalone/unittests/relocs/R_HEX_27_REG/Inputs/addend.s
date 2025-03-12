	.text
	.file	"msg.s"
	.globl	foo
	.balign 32
	.type	foo,@function
foo:                                    // @foo
	{
		r0 = iconst(#.L.str)
	}
.Ltmp0:
	.size	foo, .Ltmp0-foo

	.type	.L.str,@object          // @.str
	.section	.rodata.str1.8,"aMS",@progbits,1
.L.str0:
        .string "test"
        .size .L.str0, 5
        .p2align 3
.L.str:
	.string	"string"
	.size	.L.str, 7
