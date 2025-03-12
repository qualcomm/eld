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
        .space 1024*1024*512, 'a'
.L.str:
	.string	"string"
	.size	.L.str, 7
