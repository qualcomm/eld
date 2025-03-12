	.text
	.attribute	4, 16
	.attribute	5, "rv32i2p1_m2p0_c2p0"
        .weak mygroup
mygroup:
        .word 100
	.file	"ranges.c"
	.section	.text.foo,"axG",@progbits,mygroup,comdat
	.weak	foo
	.p2align	1
	.type	foo,@function
foo:
.Lfunc_begin0:
	.file	0 "/tmp/64bt" "ranges.c" md5 0xb371986aa8df84efaa03c1c7c3006d46
	.loc	0 1 0
	.cfi_sections .debug_frame
	.cfi_startproc
	.loc	0 1 40 prologue_end
	li	a0, 1
.Ltmp0:
	ret
.Ltmp1:
.Lfunc_end0:
	.size	foo, .Lfunc_end0-foo
	.cfi_endproc

	.section	.text.bar,"axG",@progbits,mygroup,comdat
	.weak	bar
	.p2align	1
	.type	bar,@function
bar:
.Lfunc_begin1:
	.loc	0 3 0
	.cfi_startproc
	.loc	0 3 40 prologue_end
	li	a0, 2
.Ltmp2:
	ret
.Ltmp3:
.Lfunc_end1:
	.size	bar, .Lfunc_end1-bar
	.cfi_endproc

	.section	.text.baz,"ax",@progbits
	.globl	baz
	.p2align	1
	.type	baz,@function
baz:
.Lfunc_begin2:
	.loc	0 6 0
	.cfi_startproc
	.loc	0 7 10 prologue_end
	tail	foo
.Ltmp4:
.Lfunc_end2:
	.size	baz, .Lfunc_end2-baz
	.cfi_endproc

	.section	.debug_loclists,"",@progbits
	.word	.Ldebug_list_header_end0-.Ldebug_list_header_start0
.Ldebug_list_header_start0:
	.half	5
	.byte	4
	.byte	0
	.word	2
.Lloclists_table_base0:
	.word	.Ldebug_loc0-.Lloclists_table_base0
	.word	.Ldebug_loc1-.Lloclists_table_base0
.Ldebug_loc0:
	.byte	2
	.byte	0
	.byte	3
	.byte	1
	.byte	90
	.byte	0
.Ldebug_loc1:
	.byte	2
	.byte	1
	.byte	4
	.byte	1
	.byte	90
	.byte	0
.Ldebug_list_header_end0:
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
	.ascii	"\214\001"
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
	.byte	122
	.byte	25
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
	.byte	34
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
	.byte	5
	.byte	72
	.byte	0
	.byte	127
	.byte	19
	.ascii	"\202\001"
	.byte	25
	.ascii	"\201\001"
	.byte	27
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
	.byte	4
	.word	.debug_abbrev
	.byte	1
	.byte	0
	.half	29
	.byte	1
	.word	.Lstr_offsets_base0
	.word	.Lline_table_start0
	.byte	2
	.word	0
	.byte	0
	.word	.Laddr_table_base0
	.word	.Lrnglists_table_base0
	.word	.Lloclists_table_base0
	.byte	2
	.byte	0
	.word	.Lfunc_end0-.Lfunc_begin0
	.byte	1
	.byte	82

	.byte	3
	.byte	0
	.byte	1

	.word	125

	.byte	3
	.byte	0
	.byte	7
	.byte	0
	.byte	1
	.word	125
	.byte	0
	.byte	2
	.byte	1
	.word	.Lfunc_end1-.Lfunc_begin1
	.byte	1
	.byte	82

	.byte	5
	.byte	0
	.byte	3

	.word	125

	.byte	3
	.byte	1
	.byte	7
	.byte	0
	.byte	3
	.word	125
	.byte	0
	.byte	2
	.byte	2
	.word	.Lfunc_end2-.Lfunc_begin2
	.byte	1
	.byte	82

	.byte	6
	.byte	0
	.byte	5

	.word	125

	.byte	4
	.byte	1
	.byte	90
	.byte	8
	.byte	0
	.byte	5
	.word	125
	.byte	5
	.word	43

	.byte	2
	.byte	0
	.byte	6
	.byte	4
	.byte	5
	.byte	4
	.byte	0
.Ldebug_info_end0:
	.section	.debug_rnglists,"",@progbits
	.word	.Ldebug_list_header_end1-.Ldebug_list_header_start1
.Ldebug_list_header_start1:
	.half	5
	.byte	4
	.byte	0
	.word	1
.Lrnglists_table_base0:
	.word	.Ldebug_ranges0-.Lrnglists_table_base0
.Ldebug_ranges0:
	.byte	2
	.byte	0
	.byte	5
	.byte	2
	.byte	1
	.byte	6
	.byte	2
	.byte	2
	.byte	7
	.byte	0
.Ldebug_list_header_end1:
	.section	.debug_str_offsets,"",@progbits
	.word	40
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
	.asciz	"baz"
.Linfo_string7:
	.asciz	"a"
.Linfo_string8:
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
	.section	.debug_addr,"",@progbits
	.word	.Ldebug_addr_end0-.Ldebug_addr_start0
.Ldebug_addr_start0:
	.half	5
	.byte	4
	.byte	0
.Laddr_table_base0:
	.word	.Lfunc_begin0
	.word	.Lfunc_begin1
	.word	.Lfunc_begin2
	.word	.Ltmp0
	.word	.Ltmp2
	.word	.Lfunc_end0
	.word	.Lfunc_end1
	.word	.Lfunc_end2
.Ldebug_addr_end0:
	.ident	"Snapdragon LLVM ARM Compiler 17.0.0"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.section	.debug_line,"",@progbits
.Lline_table_start0:
