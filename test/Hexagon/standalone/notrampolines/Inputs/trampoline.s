	.file	"main.s"
	.section	.text.othermain,"ax",@progbits
        .p2align 4
	.globl othermain
	.type	othermain, @function
othermain:
	allocframe(#16)
	call myfn
	deallocframe
        jumpr r31
	.size	othermain, .-othermain
	.section	.text.main,"ax",@progbits
        .p2align 4
	.globl main
	.type	main, @function
main:
	allocframe(#16)
	call myfn
	deallocframe
        jumpr r31
	.size	main, .-main

       .section .text.fn, "ax", @progbits
        .p2align 4
       .org 0x800000
       .globl myfn
       .type	myfn, @function
myfn:
	allocframe(#16)
	deallocframe
	jumpr r31
	.size	myfn, .-myfn
	.ident	"GCC: (GNU) 3.4.6 Hexagon Build Version 1.0.00 BT_20080818_1213_lnx64"
