	.section	.rodata
.LC0:
	.string	"FarFunction"

	.section .text.far,"ax",@progbits
        .p2align 4
	.globl FarFunction
	.type	FarFunction, @function
FarFunction:
	allocframe(#0)
	r0 = ##.LC0
	dealloc_return
	.size	FarFunction, .-FarFunction
