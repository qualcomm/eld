	.file	"1.i"
	.text
	.globl	main
	.balign 32
	.type	main,@function
main:
	{
		allocframe(#0)
	}
	{
		r0=memw(#str1)
	}
	{
		r0 = #0
		dealloc_return
	}
.Ltmp0:
	.size	main, .Ltmp0-main

	.globl	fn
	.balign 32
	.type	fn,@function
fn:
	{
		allocframe(#0)
	}
	{
		r0=memw(#str1)
	}
	{
		r0 = #0
		dealloc_return
	}
.Ltmp1:
	.size	fn, .Ltmp1-fn

	.type	.L.str,@object
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align	8
.L.str:
	.string	 "qualcomm"
	.size	.L.str, 9

	.type	str1,@object
	.section	.sdata.4,"aw",@progbits
	.globl	str1
	.align	4
str1:
	.word	.L.str
	.size	str1, 4

	.type	.L.str1,@object
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align	8
.L.str1:
	.string	 "is"
	.size	.L.str1, 3

	.type	str2,@object
	.section	.sdata.4,"aw",@progbits
	.globl	str2
	.align	4
str2:
	.word	.L.str1
	.size	str2, 4

	.type	.L.str2,@object
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align	8
.L.str2:
	.string	 "great"
	.size	.L.str2, 6

	.type	str3,@object
	.section	.sdata.4,"aw",@progbits
	.globl	str3
	.align	4
str3:
	.word	.L.str2
	.size	str3, 4


