	.file	"1.c"
	.globl dst
	.section	.tdata,"awT",@progbits
	.p2align 2
	.type	dst, @object
	.size	dst, 4
dst:
	.word	4
	.section	.bss,"aw",@nobits
	.comm	ptr,4,4
	.text
	.p2align 2
	.globl main
	.type	main, @function
main:
	// pretend args regs size (bytes) = 0
	// saved LR + FP regs size (bytes) = 8
	// callee saved regs size (bytes) = 8
	// local vars size (bytes) = 0
	// fixed args size (bytes) = 0
	allocframe(#8)
	memd(r29+#0) = r25:24
	r24 = add(pc,##_GLOBAL_OFFSET_TABLE_@PCREL)
	r25 = ugp
	r0 = memw(r24+##ptr@GOT)
	r1 = memw(r0+#0)
	r0.h = #HI(src1@IEGOT)
	r0.l = #LO(src1@IEGOT)
	r0 = add(r0,r24)
	r0 = memw(r0+#0)
	r0 = add(r0,r25)
	r2 = memw(r0+#0)
	r0.h = #HI(src2@IEGOT)
	r0.l = #LO(src2@IEGOT)
	r0 = add(r0,r24)
	r0 = memw(r0+#0)
	r0 = add(r0,r25)
	r0 = memw(r0+#0)
	r0 = add(r2,r0)
	memw(r1+#0) = r0
	r0 = memw(r24+##ptr@GOT)
	r0 = memw(r0+#0)
	r0 = memw(r0+#0)
	r25:24 = memd(r29+#0)
	deallocframe
	jumpr r31
	.size	main, .-main
	.ident	"GCC: (Sourcery CodeBench Lite 5.1 2012.03-137) 4.6.1"
	.section	.note.GNU-stack,"",@progbits
