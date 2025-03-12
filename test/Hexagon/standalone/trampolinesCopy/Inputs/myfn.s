       .section .boo, "ax", @progbits
        .p2align 2
       .globl myfn
       .type	myfn, @function
myfn:
	allocframe(#16)
	deallocframe
	jumpr r31
	.size	myfn, .-myfn
