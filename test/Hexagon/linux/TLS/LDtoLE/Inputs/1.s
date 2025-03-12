	.file	"1.c"
	.section	.bss,"aw",@nobits
	.comm	ptr,4,4
	.text
	.p2align 2
	.globl foo
	.type	foo, @function
foo:
	// pretend args regs size (bytes) = 0
	// saved LR + FP regs size (bytes) = 8
	// callee saved regs size (bytes) = 8
	// local vars size (bytes) = 0
	// fixed args size (bytes) = 0
	allocframe(#8)	//
	memw(r29+#4) = r16	//,
	memw(r29+#0) = r24	//,
	r24 = add(pc,##_GLOBAL_OFFSET_TABLE_@PCREL)	//
	call bar@PLT	//
	r16 = r0	// D.3684,
	r0.h = #HI(src1@LDGOT)	// tmp60,
	r0.l = #LO(src1@LDGOT)	// tmp60,
	r0 = add(r0,r24)	// tmp60, tmp60,
	call src1@LDPLT	//
	r1 = r0	// tmp61,
	r0.h = #HI(src1@DTPREL)	// tmp62,
	r0.l = #LO(src1@DTPREL)	// tmp62,
	r0 = add(r0,r1)	// tmp62, tmp62, tmp61
	r0 = memw(r0+#0)	// src1.0, src1
	r1 = add(r16,r0)	// ptr.1, D.3684, src1.0
	r0 = memw(r24+##ptr@GOT)	// tmp63,,
	memw(r0+#0) = r1	// ptr, ptr.1
	r0 = memw(r24+##ptr@GOT)	// tmp65,,
	r0 = memw(r0+#0)	// D.3687, ptr
	r24 = memw(r29+#0)	//,
	r16 = memw(r29+#4)	//,
	deallocframe
	jumpr r31
	.size	foo, .-foo
	.ident	"GCC: (Sourcery CodeBench Lite 5.1 2012.03-137) 4.6.1"
	.section	.note.GNU-stack,"",@progbits
