	.text
	.globl	foo
	.balign 32
	.type	foo,@function
foo:
	{
		r0 = #0
		jumpr r31
	}
.Ltmp0:
	.size	foo, .Ltmp0-foo
