	.section .text.farA,"ax",@progbits
        .p2align 4
	.globl FarFunctionA
	.type	FarFunctionA, @function
FarFunctionA:
	allocframe(#0)
	dealloc_return
	.size	FarFunctionA, .-FarFunctionA
	.section .text.farB,"ax",@progbits
        .p2align 4
