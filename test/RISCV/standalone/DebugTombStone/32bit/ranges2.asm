	.attribute	5, "rv32i2p1_m2p0_c2p0"
	.file	"ranges.c"
        .weak mygroup
mygroup:
        .word 100
	.section	.text.foo,"axG",@progbits,mygroup,comdat
	.globl	foo
	.p2align	1
	.type	foo,@function
foo:
.Lfunc_begin0:
	.file	1 "/tmp/64bt" "ranges.c"
	.loc	1 1 0
	addi	sp, sp, -16
	sw	ra, 12(sp)
	sw	s0, 8(sp)
	addi	s0, sp, 16
	sw	a0, -12(s0)
	li	a0, 1
.Ltmp0:
	.loc	1 1 53 prologue_end
	lw	ra, 12(sp)
	lw	s0, 8(sp)
	addi	sp, sp, 16
	ret
.Ltmp1:
.Lfunc_end0:
	.size	foo, .Lfunc_end0-foo

	.section	.text.bar,"ax",@progbits
	.globl	bar
	.p2align	1
	.type	bar,@function
bar:
.Lfunc_begin1:
	.loc	1 3 0
	addi	sp, sp, -16
	sw	ra, 12(sp)
	sw	s0, 8(sp)
	addi	s0, sp, 16
	sw	a0, -12(s0)
	li	a0, 2
.Ltmp2:
	.loc	1 3 53 prologue_end
	lw	ra, 12(sp)
	lw	s0, 8(sp)
	addi	sp, sp, 16
	ret
.Ltmp3:
.Lfunc_end1:
	.size	bar, .Lfunc_end1-bar

	.section	.text.boo,"ax",@progbits
	.globl	boo
	.p2align	1
	.type	boo,@function
boo:
.Lfunc_begin2:
	.loc	1 5 0
	addi	sp, sp, -16
	sw	ra, 12(sp)
	sw	s0, 8(sp)
	addi	s0, sp, 16
	sw	a0, -12(s0)
	li	a0, 3
.Ltmp4:
	.loc	1 5 53 prologue_end
	lw	ra, 12(sp)
	lw	s0, 8(sp)
	addi	sp, sp, 16
	ret
.Ltmp5:
.Lfunc_end2:
	.size	boo, .Lfunc_end2-boo

	.section	.text.coo,"ax",@progbits
	.weak	coo
	.p2align	1
	.type	coo,@function
coo:
.Lfunc_begin3:
	.loc	1 7 0
	addi	sp, sp, -16
	sw	ra, 12(sp)
	sw	s0, 8(sp)
	addi	s0, sp, 16
	li	a0, 0
.Ltmp6:
	.loc	1 7 61 prologue_end
	lw	ra, 12(sp)
	lw	s0, 8(sp)
	addi	sp, sp, 16
	ret
.Ltmp7:
.Lfunc_end3:
	.size	coo, .Lfunc_end3-coo

	.section	.text.zoo,"ax",@progbits
	.globl	zoo
	.p2align	1
	.type	zoo,@function
zoo:
.Lfunc_begin4:
	.loc	1 9 0
	addi	sp, sp, -16
	sw	ra, 12(sp)
	sw	s0, 8(sp)
	addi	s0, sp, 16
	sw	a0, -12(s0)
	li	a0, 3
.Ltmp8:
	.loc	1 9 53 prologue_end
	lw	ra, 12(sp)
	lw	s0, 8(sp)
	addi	sp, sp, 16
	ret
.Ltmp9:
.Lfunc_end4:
	.size	zoo, .Lfunc_end4-zoo

	.section	.text.baz,"ax",@progbits
	.globl	baz
	.p2align	1
	.type	baz,@function
baz:
.Lfunc_begin5:
	.loc	1 12 0
	addi	sp, sp, -32
	sw	ra, 28(sp)
	sw	s0, 24(sp)
	addi	s0, sp, 32
	sw	a0, -12(s0)
.Ltmp10:
	.loc	1 13 14 prologue_end
	lw	a0, -12(s0)
	.loc	1 13 10 is_stmt 0
	call	foo
	.loc	1 13 26
	sw	a0, -20(s0)
	lw	a0, -12(s0)
	.loc	1 13 22
	call	boo
	mv	a1, a0
	.loc	1 13 20
	lw	a0, -20(s0)
	add	a0, a0, a1
	.loc	1 13 39
	sw	a0, -16(s0)
	lw	a0, -12(s0)
	.loc	1 13 35
	call	zoo
	mv	a1, a0
	.loc	1 13 32
	lw	a0, -16(s0)
	add	a0, a0, a1
	.loc	1 13 3
	lw	ra, 28(sp)
	lw	s0, 24(sp)
	addi	sp, sp, 32
	ret
.Ltmp11:
.Lfunc_end5:
	.size	baz, .Lfunc_end5-baz

	.section	.debug_abbrev,"",@progbits
	.byte	1
	.byte	17
	.byte	1
	.byte	37
	.byte	14
	.byte	19
	.byte	5
	.byte	3
	.byte	14
	.byte	16
	.byte	23
	.byte	27
	.byte	14
	.byte	17
	.byte	1
	.byte	85
	.byte	23
	.byte	0
	.byte	0
	.byte	2
	.byte	46
	.byte	1
	.byte	17
	.byte	1
	.byte	18
	.byte	6
	.byte	64
	.byte	24
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	39
	.byte	25
	.byte	73
	.byte	19
	.byte	63
	.byte	25
	.byte	0
	.byte	0
	.byte	3
	.byte	5
	.byte	0
	.byte	2
	.byte	24
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	4
	.byte	46
	.byte	0
	.byte	17
	.byte	1
	.byte	18
	.byte	6
	.byte	64
	.byte	24
	.byte	3
	.byte	14
	.byte	58
	.byte	11
	.byte	59
	.byte	11
	.byte	73
	.byte	19
	.byte	63
	.byte	25
	.byte	0
	.byte	0
	.byte	5
	.byte	53
	.byte	0
	.byte	73
	.byte	19
	.byte	0
	.byte	0
	.byte	6
	.byte	36
	.byte	0
	.byte	3
	.byte	14
	.byte	62
	.byte	11
	.byte	11
	.byte	11
	.byte	0
	.byte	0
	.byte	0
	.section	.debug_info,"",@progbits
.Lcu_begin0:
	.word	.Ldebug_info_end0-.Ldebug_info_start0
.Ldebug_info_start0:
	.half	4
	.word	.debug_abbrev
	.byte	4
	.byte	1
	.word	.Linfo_string0
	.half	12
	.word	.Linfo_string1
	.word	.Lline_table_start0
	.word	.Linfo_string2
	.word	0
	.word	.Ldebug_ranges0
	.byte	2
	.word	.Lfunc_begin0
	.word	.Lfunc_end0-.Lfunc_begin0
	.byte	1
	.byte	88
	.word	.Linfo_string3
	.byte	1
	.byte	1

	.word	239

	.byte	3
	.byte	2
	.byte	145
	.byte	116
	.word	.Linfo_string10
	.byte	1
	.byte	1
	.word	244
	.byte	0
	.byte	2
	.word	.Lfunc_begin1
	.word	.Lfunc_end1-.Lfunc_begin1
	.byte	1
	.byte	88
	.word	.Linfo_string5
	.byte	1
	.byte	3

	.word	239

	.byte	3
	.byte	2
	.byte	145
	.byte	116
	.word	.Linfo_string10
	.byte	1
	.byte	3
	.word	244
	.byte	0
	.byte	2
	.word	.Lfunc_begin2
	.word	.Lfunc_end2-.Lfunc_begin2
	.byte	1
	.byte	88
	.word	.Linfo_string6
	.byte	1
	.byte	5

	.word	239

	.byte	3
	.byte	2
	.byte	145
	.byte	116
	.word	.Linfo_string10
	.byte	1
	.byte	5
	.word	244
	.byte	0
	.byte	4
	.word	.Lfunc_begin3
	.word	.Lfunc_end3-.Lfunc_begin3
	.byte	1
	.byte	88
	.word	.Linfo_string7
	.byte	1
	.byte	7
	.word	244

	.byte	2
	.word	.Lfunc_begin4
	.word	.Lfunc_end4-.Lfunc_begin4
	.byte	1
	.byte	88
	.word	.Linfo_string8
	.byte	1
	.byte	9

	.word	239

	.byte	3
	.byte	2
	.byte	145
	.byte	116
	.word	.Linfo_string10
	.byte	1
	.byte	9
	.word	244
	.byte	0
	.byte	2
	.word	.Lfunc_begin5
	.word	.Lfunc_end5-.Lfunc_begin5
	.byte	1
	.byte	88
	.word	.Linfo_string9
	.byte	1
	.byte	11

	.word	244

	.byte	3
	.byte	2
	.byte	145
	.byte	116
	.word	.Linfo_string11
	.byte	1
	.byte	11
	.word	244
	.byte	0
	.byte	5
	.word	244
	.byte	6
	.word	.Linfo_string4
	.byte	5
	.byte	4
	.byte	0
.Ldebug_info_end0:
	.section	.debug_ranges,"",@progbits
.Ldebug_ranges0:
	.word	.Lfunc_begin0
	.word	.Lfunc_end0
	.word	.Lfunc_begin1
	.word	.Lfunc_end1
	.word	.Lfunc_begin2
	.word	.Lfunc_end2
	.word	.Lfunc_begin3
	.word	.Lfunc_end3
	.word	.Lfunc_begin4
	.word	.Lfunc_end4
	.word	.Lfunc_begin5
	.word	.Lfunc_end5
	.word	0
	.word	0
	.section	.debug_str,"MS",@progbits,1
.Linfo_string0:
	.asciz	"ClangBuiltLinux clang version 15.0.7 (https://github.com/llvm/llvm-project 8dfdcc7b7bf66834a761bd8de445840ef68e4d1a)"
.Linfo_string1:
	.asciz	"ranges.c"
.Linfo_string2:
	.asciz	"/tmp/64bt"
.Linfo_string3:
	.asciz	"foo"
.Linfo_string4:
	.asciz	"int"
.Linfo_string5:
	.asciz	"bar"
.Linfo_string6:
	.asciz	"boo"
.Linfo_string7:
	.asciz	"coo"
.Linfo_string8:
	.asciz	"zoo"
.Linfo_string9:
	.asciz	"baz"
.Linfo_string10:
	.asciz	"a"
.Linfo_string11:
	.asciz	"argc"
	.ident	"ClangBuiltLinux clang version 15.0.7 (https://github.com/llvm/llvm-project 8dfdcc7b7bf66834a761bd8de445840ef68e4d1a)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym foo
	.addrsig_sym boo
	.addrsig_sym zoo
	.section	.debug_line,"",@progbits
.Lline_table_start0:
