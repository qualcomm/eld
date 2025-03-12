	.text
	.globl	baz
	.balign 32
	.type	baz,@function
baz:
	{
		r0 = #0
		jumpr r31
	}
.Ltmp0:
	.size	baz, .Ltmp0-baz
