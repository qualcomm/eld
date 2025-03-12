	.file	"2.c"
	.globl src2
	.section	.tdata,"awT",@progbits
	.p2align 2
	.type	src2, @object
	.size	src2, 4
src2:
	.word	10
	.globl src1
	.p2align 2
	.type	src1, @object
	.size	src1, 4
src1:
	.word	10
	.p2align 2
	.type	staticdata, @object
	.size	staticdata, 4
staticdata:
	.word	10
	.text
	.p2align 2
	.globl bar
	.type	bar, @function
bar:
	// pretend args regs size (bytes) = 0
	// saved LR + FP regs size (bytes) = 8
	// callee saved regs size (bytes) = 8
	// local vars size (bytes) = 0
	// fixed args size (bytes) = 0
	allocframe(#8)	//
	memw(r29+#4) = r16	//,
	memw(r29+#0) = r24	//,
	r24 = add(pc,##_GLOBAL_OFFSET_TABLE_@PCREL)	//
	r0.h = #HI(src2@LDGOT)	// tmp59,
	r0.l = #LO(src2@LDGOT)	// tmp59,
	r0 = add(r0,r24)	// tmp59, tmp59,
	call src2@LDPLT	//
	r1 = r0	// tmp60,
	r0.h = #HI(src2@DTPREL)	// tmp61,
	r0.l = #LO(src2@DTPREL)	// tmp61,
	r0 = add(r0,r1)	// tmp61, tmp61, tmp60
	r16 = memw(r0+#0)	// src2.0, src2
	r0.h = #HI(staticdata@LDGOT)	// tmp62,
	r0.l = #LO(staticdata@LDGOT)	// tmp62,
	r0 = add(r0,r24)	// tmp62, tmp62,
	call staticdata@LDPLT	//
	r1 = r0	// tmp63,
	r0.h = #HI(staticdata@DTPREL)	// tmp64,
	r0.l = #LO(staticdata@DTPREL)	// tmp64,
	r0 = add(r0,r1)	// tmp64, tmp64, tmp63
	r0 = memw(r0+#0)	// staticdata.1, staticdata
	r0 = add(r16,r0)	// D.3683, src2.0, staticdata.1
	r24 = memw(r29+#0)	//,
	r16 = memw(r29+#4)	//,
	deallocframe
	jumpr r31
	.size	bar, .-bar
	.ident	"GCC: (Sourcery CodeBench Lite 5.1 2012.03-137) 4.6.1"
	.section	.note.GNU-stack,"",@progbits
