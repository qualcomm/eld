	.text
	.globl	bar
	.balign 32
	.type	bar,@function
bar:
	{
		r0 = #0
		jumpr r31
	}
.Ltmp0:
	.size	bar, .Ltmp0-bar
