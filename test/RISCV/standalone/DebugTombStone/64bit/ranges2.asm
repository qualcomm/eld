	.text
	.attribute	4, 16
	.attribute	5, "rv64i2p1_m2p0_c2p0"
	.file	"ranges.c"
        .weak mygroup
mygroup:
        .quad 100
	.section	.text.foo,"axG",@progbits,mygroup,comdat
	.globl	foo
	.p2align	1
	.type	foo,@function
foo:
.Lfunc_begin0:
	.file	0 "/tmp/64bt" "ranges.c" md5 0x64638270d0b95af174d0648533f42356
	.loc	0 1 0
	.cfi_sections .debug_frame
	.cfi_startproc
	addi	sp, sp, -32
	.cfi_def_cfa_offset 32
	sd	ra, 24(sp)
	sd	s0, 16(sp)
	.cfi_offset ra, -8
	.cfi_offset s0, -16
	addi	s0, sp, 32
	.cfi_def_cfa s0, 0
	sw	a0, -20(s0)
	li	a0, 1
.Ltmp0:
	.loc	0 1 53 prologue_end
	ld	ra, 24(sp)
	ld	s0, 16(sp)
	.loc	0 1 53 epilogue_begin is_stmt 0
	addi	sp, sp, 32
	ret
.Ltmp1:
.Lfunc_end0:
	.size	foo, .Lfunc_end0-foo
	.cfi_endproc

	.section	.text.bar,"ax",@progbits
	.globl	bar
	.p2align	1
	.type	bar,@function
bar:
.Lfunc_begin1:
	.loc	0 3 0 is_stmt 1
	.cfi_startproc
	addi	sp, sp, -32
	.cfi_def_cfa_offset 32
	sd	ra, 24(sp)
	sd	s0, 16(sp)
	.cfi_offset ra, -8
	.cfi_offset s0, -16
	addi	s0, sp, 32
	.cfi_def_cfa s0, 0
	sw	a0, -20(s0)
	li	a0, 2
.Ltmp2:
	.loc	0 3 53 prologue_end
	ld	ra, 24(sp)
	ld	s0, 16(sp)
	.loc	0 3 53 epilogue_begin is_stmt 0
	addi	sp, sp, 32
	ret
.Ltmp3:
.Lfunc_end1:
	.size	bar, .Lfunc_end1-bar
	.cfi_endproc

	.section	.text.boo,"ax",@progbits
	.globl	boo
	.p2align	1
	.type	boo,@function
boo:
.Lfunc_begin2:
	.loc	0 5 0 is_stmt 1
	.cfi_startproc
	addi	sp, sp, -32
	.cfi_def_cfa_offset 32
	sd	ra, 24(sp)
	sd	s0, 16(sp)
	.cfi_offset ra, -8
	.cfi_offset s0, -16
	addi	s0, sp, 32
	.cfi_def_cfa s0, 0
	sw	a0, -20(s0)
	li	a0, 3
.Ltmp4:
	.loc	0 5 53 prologue_end
	ld	ra, 24(sp)
	ld	s0, 16(sp)
	.loc	0 5 53 epilogue_begin is_stmt 0
	addi	sp, sp, 32
	ret
.Ltmp5:
.Lfunc_end2:
	.size	boo, .Lfunc_end2-boo
	.cfi_endproc

	.section	.text.coo,"ax",@progbits
	.weak	coo
	.p2align	1
	.type	coo,@function
coo:
.Lfunc_begin3:
	.loc	0 7 0 is_stmt 1
	.cfi_startproc
	addi	sp, sp, -16
	.cfi_def_cfa_offset 16
	sd	ra, 8(sp)
	sd	s0, 0(sp)
	.cfi_offset ra, -8
	.cfi_offset s0, -16
	addi	s0, sp, 16
	.cfi_def_cfa s0, 0
	li	a0, 0
.Ltmp6:
	.loc	0 7 61 prologue_end
	ld	ra, 8(sp)
	ld	s0, 0(sp)
	.loc	0 7 61 epilogue_begin is_stmt 0
	addi	sp, sp, 16
	ret
.Ltmp7:
.Lfunc_end3:
	.size	coo, .Lfunc_end3-coo
	.cfi_endproc

	.section	.text.zoo,"ax",@progbits
	.globl	zoo
	.p2align	1
	.type	zoo,@function
zoo:
.Lfunc_begin4:
	.loc	0 9 0 is_stmt 1
	.cfi_startproc
	addi	sp, sp, -32
	.cfi_def_cfa_offset 32
	sd	ra, 24(sp)
	sd	s0, 16(sp)
	.cfi_offset ra, -8
	.cfi_offset s0, -16
	addi	s0, sp, 32
	.cfi_def_cfa s0, 0
	sw	a0, -20(s0)
	li	a0, 3
.Ltmp8:
	.loc	0 9 53 prologue_end
	ld	ra, 24(sp)
	ld	s0, 16(sp)
	.loc	0 9 53 epilogue_begin is_stmt 0
	addi	sp, sp, 32
	ret
.Ltmp9:
.Lfunc_end4:
	.size	zoo, .Lfunc_end4-zoo
	.cfi_endproc

	.section	.text.baz,"ax",@progbits
	.globl	baz
	.p2align	1
	.type	baz,@function
baz:
.Lfunc_begin5:
	.loc	0 12 0 is_stmt 1
	.cfi_startproc
	addi	sp, sp, -48
	.cfi_def_cfa_offset 48
	sd	ra, 40(sp)
	sd	s0, 32(sp)
	.cfi_offset ra, -8
	.cfi_offset s0, -16
	addi	s0, sp, 48
	.cfi_def_cfa s0, 0
	sw	a0, -20(s0)
.Ltmp10:
	.loc	0 13 14 prologue_end
	lw	a0, -20(s0)
	.loc	0 13 10 is_stmt 0
	call	foo
	.loc	0 13 26
	sd	a0, -40(s0)
	lw	a0, -20(s0)
	.loc	0 13 22
	call	boo
	mv	a1, a0
	.loc	0 13 20
	ld	a0, -40(s0)
	addw	a0, a0, a1
	.loc	0 13 39
	sd	a0, -32(s0)
	lw	a0, -20(s0)
	.loc	0 13 35
	call	zoo
	mv	a1, a0
	.loc	0 13 32
	ld	a0, -32(s0)
	addw	a0, a0, a1
	.loc	0 13 3
	ld	ra, 40(sp)
	ld	s0, 32(sp)
	.loc	0 13 3 epilogue_begin
	addi	sp, sp, 48
	ret
.Ltmp11:
.Lfunc_end5:
	.size	baz, .Lfunc_end5-baz
	.cfi_endproc

	.section	.debug_abbrev,"",@progbits
	.byte	1
	.byte	17
	.byte	1
	.byte	37
	.byte	37
	.byte	19
	.byte	5
	.byte	3
	.byte	37
	.byte	114
	.byte	23
	.byte	16
	.byte	23
	.byte	27
	.byte	37
	.byte	17
	.byte	1
	.byte	85
	.byte	35
	.byte	115
	.byte	23
	.byte	116
	.byte	23
	.byte	0
	.byte	0
	.byte	2
	.byte	46
	.byte	1
	.byte	17
	.byte	27
	.byte	18
	.byte	6
	.byte	64
	.byte	24
	.byte	3
	.byte	37
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
	.byte	37
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
	.byte	27
	.byte	18
	.byte	6
	.byte	64
	.byte	24
	.byte	3
	.byte	37
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
	.byte	37
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
	.half	5
	.byte	1
	.byte	8
	.word	.debug_abbrev
	.byte	1
	.byte	0
	.half	29
	.byte	1
	.word	.Lstr_offsets_base0
	.word	.Lline_table_start0
	.byte	2
	.quad	0
	.byte	0
	.word	.Laddr_table_base0
	.word	.Lrnglists_table_base0
	.byte	2
	.byte	0
	.word	.Lfunc_end0-.Lfunc_begin0
	.byte	1
	.byte	88
	.byte	3
	.byte	0
	.byte	1

	.word	193

	.byte	3
	.byte	2
	.byte	145
	.byte	108
	.byte	10
	.byte	0
	.byte	1
	.word	198
	.byte	0
	.byte	2
	.byte	1
	.word	.Lfunc_end1-.Lfunc_begin1
	.byte	1
	.byte	88
	.byte	5
	.byte	0
	.byte	3

	.word	193

	.byte	3
	.byte	2
	.byte	145
	.byte	108
	.byte	10
	.byte	0
	.byte	3
	.word	198
	.byte	0
	.byte	2
	.byte	2
	.word	.Lfunc_end2-.Lfunc_begin2
	.byte	1
	.byte	88
	.byte	6
	.byte	0
	.byte	5

	.word	193

	.byte	3
	.byte	2
	.byte	145
	.byte	108
	.byte	10
	.byte	0
	.byte	5
	.word	198
	.byte	0
	.byte	4
	.byte	3
	.word	.Lfunc_end3-.Lfunc_begin3
	.byte	1
	.byte	88
	.byte	7
	.byte	0
	.byte	7
	.word	198

	.byte	2
	.byte	4
	.word	.Lfunc_end4-.Lfunc_begin4
	.byte	1
	.byte	88
	.byte	8
	.byte	0
	.byte	9

	.word	193

	.byte	3
	.byte	2
	.byte	145
	.byte	108
	.byte	10
	.byte	0
	.byte	9
	.word	198
	.byte	0
	.byte	2
	.byte	5
	.word	.Lfunc_end5-.Lfunc_begin5
	.byte	1
	.byte	88
	.byte	9
	.byte	0
	.byte	11

	.word	198

	.byte	3
	.byte	2
	.byte	145
	.byte	108
	.byte	11
	.byte	0
	.byte	11
	.word	198
	.byte	0
	.byte	5
	.word	198
	.byte	6
	.byte	4
	.byte	5
	.byte	4
	.byte	0
.Ldebug_info_end0:
	.section	.debug_rnglists,"",@progbits
	.word	.Ldebug_list_header_end0-.Ldebug_list_header_start0
.Ldebug_list_header_start0:
	.half	5
	.byte	8
	.byte	0
	.word	1
.Lrnglists_table_base0:
	.word	.Ldebug_ranges0-.Lrnglists_table_base0
.Ldebug_ranges0:
	.byte	2
	.byte	0
	.byte	6
	.byte	2
	.byte	1
	.byte	7
	.byte	2
	.byte	2
	.byte	8
	.byte	2
	.byte	3
	.byte	9
	.byte	2
	.byte	4
	.byte	10
	.byte	2
	.byte	5
	.byte	11
	.byte	0
.Ldebug_list_header_end0:
	.section	.debug_str_offsets,"",@progbits
	.word	52
	.half	5
	.half	0
.Lstr_offsets_base0:
	.section	.debug_str,"MS",@progbits,1
.Linfo_string0:
	.asciz	"Snapdragon LLVM ARM Compiler 17.0.0"
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
	.section	.debug_str_offsets,"",@progbits
	.word	.Linfo_string0
	.word	.Linfo_string1
	.word	.Linfo_string2
	.word	.Linfo_string3
	.word	.Linfo_string4
	.word	.Linfo_string5
	.word	.Linfo_string6
	.word	.Linfo_string7
	.word	.Linfo_string8
	.word	.Linfo_string9
	.word	.Linfo_string10
	.word	.Linfo_string11
	.section	.debug_addr,"",@progbits
	.word	.Ldebug_addr_end0-.Ldebug_addr_start0
.Ldebug_addr_start0:
	.half	5
	.byte	8
	.byte	0
.Laddr_table_base0:
	.quad	.Lfunc_begin0
	.quad	.Lfunc_begin1
	.quad	.Lfunc_begin2
	.quad	.Lfunc_begin3
	.quad	.Lfunc_begin4
	.quad	.Lfunc_begin5
	.quad	.Lfunc_end0
	.quad	.Lfunc_end1
	.quad	.Lfunc_end2
	.quad	.Lfunc_end3
	.quad	.Lfunc_end4
	.quad	.Lfunc_end5
.Ldebug_addr_end0:
	.ident	"Snapdragon LLVM ARM Compiler 17.0.0"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym foo
	.addrsig_sym boo
	.addrsig_sym zoo
	.section	.debug_line,"",@progbits
.Lline_table_start0:
