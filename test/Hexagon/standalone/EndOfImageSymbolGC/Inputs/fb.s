	.text
	.file	"fb.c"
	.section	.text.main,"ax",@progbits
	.globl	main
	.falign
	.type	main,@function
main:                                   // @main
.Lfunc_begin0:
	.file	1 "fb.c"
	.loc	1 1 0                   // fb.c:1:0
	.cfi_startproc
// BB#0:                                // %entry
	.loc	1 1 14 prologue_end     // fb.c:1:14
	{
		r0=#0
		jumpr r31
	}
.Ltmp0:
.Lfunc_end0:
.Ltmp1:
	.size	main, .Ltmp1-main
	.cfi_endproc

	.section	.text.foo
	.globl	foo
	.falign
	.type	foo,@function
foo:                                    // @foo
.Lfunc_begin1:
	.loc	1 2 0                   // fb.c:2:0
	.cfi_startproc
// BB#0:                                // %entry
	.loc	1 2 13 prologue_end     // fb.c:2:13
	{
		jumpr r31
	}
.Ltmp2:
.Lfunc_end1:
.Ltmp3:
	.size	foo, .Ltmp3-foo
	.cfi_endproc

	.section	.text.bar
	.globl	bar
	.falign
	.type	bar,@function
bar:                                    // @bar
.Lfunc_begin2:
	.loc	1 3 0                   // fb.c:3:0
	.cfi_startproc
// BB#0:                                // %entry
	.loc	1 3 13 prologue_end     // fb.c:3:13
	{
		jumpr r31
	}
.Ltmp4:
.Lfunc_end2:
.Ltmp5:
	.size	bar, .Ltmp5-bar
	.cfi_endproc

	.section	.debug_str,"MS",@progbits,1
.Linfo_string0:
	.string	"QuIC LLVM Hexagon Clang version hexagon-clang-81-3364 (based on LLVM 4.0.0)" // string offset=0
.Linfo_string1:
	.string	"fb.c"                  // string offset=76
.Linfo_string2:
	.string	"/tmp/shankar"          // string offset=81
.Linfo_string3:
	.string	"main"                  // string offset=94
.Linfo_string4:
	.string	"int"                   // string offset=99
.Linfo_string5:
	.string	"foo"                   // string offset=103
.Linfo_string6:
	.string	"bar"                   // string offset=107
	.section	.debug_loc,"",@progbits
	.section	.debug_abbrev,"",@progbits
.Lsection_abbrev:
	.byte	1                       // Abbreviation Code
	.byte	17                      // DW_TAG_compile_unit
	.byte	1                       // DW_CHILDREN_yes
	.byte	37                      // DW_AT_producer
	.byte	14                      // DW_FORM_strp
	.byte	19                      // DW_AT_language
	.byte	5                       // DW_FORM_data2
	.byte	3                       // DW_AT_name
	.byte	14                      // DW_FORM_strp
	.byte	16                      // DW_AT_stmt_list
	.byte	23                      // DW_FORM_sec_offset
	.byte	27                      // DW_AT_comp_dir
	.byte	14                      // DW_FORM_strp
	.byte	17                      // DW_AT_low_pc
	.byte	1                       // DW_FORM_addr
	.byte	85                      // DW_AT_ranges
	.byte	23                      // DW_FORM_sec_offset
	.byte	0                       // EOM(1)
	.byte	0                       // EOM(2)
	.byte	2                       // Abbreviation Code
	.byte	46                      // DW_TAG_subprogram
	.byte	0                       // DW_CHILDREN_no
	.byte	17                      // DW_AT_low_pc
	.byte	1                       // DW_FORM_addr
	.byte	18                      // DW_AT_high_pc
	.byte	6                       // DW_FORM_data4
	.byte	64                      // DW_AT_frame_base
	.byte	24                      // DW_FORM_exprloc
	.byte	3                       // DW_AT_name
	.byte	14                      // DW_FORM_strp
	.byte	58                      // DW_AT_decl_file
	.byte	11                      // DW_FORM_data1
	.byte	59                      // DW_AT_decl_line
	.byte	11                      // DW_FORM_data1
	.byte	73                      // DW_AT_type
	.byte	19                      // DW_FORM_ref4
	.byte	63                      // DW_AT_external
	.byte	25                      // DW_FORM_flag_present
	.byte	0                       // EOM(1)
	.byte	0                       // EOM(2)
	.byte	3                       // Abbreviation Code
	.byte	36                      // DW_TAG_base_type
	.byte	0                       // DW_CHILDREN_no
	.byte	3                       // DW_AT_name
	.byte	14                      // DW_FORM_strp
	.byte	62                      // DW_AT_encoding
	.byte	11                      // DW_FORM_data1
	.byte	11                      // DW_AT_byte_size
	.byte	11                      // DW_FORM_data1
	.byte	0                       // EOM(1)
	.byte	0                       // EOM(2)
	.byte	0                       // EOM(3)
	.section	.debug_info,"",@progbits
.Lsection_info:
.Lcu_begin0:
	.word	105                     // Length of Unit
	.half	4                       // DWARF version number
	.word	.Lsection_abbrev        // Offset Into Abbrev. Section
	.byte	4                       // Address Size (in bytes)
	.byte	1                       // Abbrev [1] 0xb:0x62 DW_TAG_compile_unit
	.word	.Linfo_string0          // DW_AT_producer
	.half	12                      // DW_AT_language
	.word	.Linfo_string1          // DW_AT_name
	.word	.Lline_table_start0     // DW_AT_stmt_list
	.word	.Linfo_string2          // DW_AT_comp_dir
	.word	0                       // DW_AT_low_pc
	.word	.Ldebug_ranges0         // DW_AT_ranges
	.byte	2                       // Abbrev [2] 0x26:0x15 DW_TAG_subprogram
	.word	.Lfunc_begin0           // DW_AT_low_pc
	.word	.Lfunc_end0-.Lfunc_begin0 // DW_AT_high_pc
	.byte	1                       // DW_AT_frame_base
	.byte	109
	.word	.Linfo_string3          // DW_AT_name
	.byte	1                       // DW_AT_decl_file
	.byte	1                       // DW_AT_decl_line
	.word	101                     // DW_AT_type
                                        // DW_AT_external
	.byte	2                       // Abbrev [2] 0x3b:0x15 DW_TAG_subprogram
	.word	.Lfunc_begin1           // DW_AT_low_pc
	.word	.Lfunc_end1-.Lfunc_begin1 // DW_AT_high_pc
	.byte	1                       // DW_AT_frame_base
	.byte	109
	.word	.Linfo_string5          // DW_AT_name
	.byte	1                       // DW_AT_decl_file
	.byte	2                       // DW_AT_decl_line
	.word	101                     // DW_AT_type
                                        // DW_AT_external
	.byte	2                       // Abbrev [2] 0x50:0x15 DW_TAG_subprogram
	.word	.Lfunc_begin2           // DW_AT_low_pc
	.word	.Lfunc_end2-.Lfunc_begin2 // DW_AT_high_pc
	.byte	1                       // DW_AT_frame_base
	.byte	109
	.word	.Linfo_string6          // DW_AT_name
	.byte	1                       // DW_AT_decl_file
	.byte	3                       // DW_AT_decl_line
	.word	101                     // DW_AT_type
                                        // DW_AT_external
	.byte	3                       // Abbrev [3] 0x65:0x7 DW_TAG_base_type
	.word	.Linfo_string4          // DW_AT_name
	.byte	5                       // DW_AT_encoding
	.byte	4                       // DW_AT_byte_size
	.byte	0                       // End Of Children Mark
	.section	.debug_ranges,"",@progbits
.Ldebug_range:
.Ldebug_ranges0:
	.word	.Lfunc_begin0
	.word	.Lfunc_end0
	.word	.Lfunc_begin1
	.word	.Lfunc_end1
	.word	.Lfunc_begin2
	.word	.Lfunc_end2
	.word	0
	.word	0
	.section	.debug_macinfo,"",@progbits
.Ldebug_macinfo:
.Lcu_macro_begin0:
	.byte	0                       // End Of Macro List Mark
	.section	.debug_pubnames,"",@progbits
	.word	.LpubNames_end0-.LpubNames_begin0 // Length of Public Names Info
.LpubNames_begin0:
	.half	2                       // DWARF Version
	.word	.Lcu_begin0             // Offset of Compilation Unit Info
	.word	109                     // Compilation Unit Length
	.word	59                      // DIE offset
	.string	"foo"                   // External Name
	.word	38                      // DIE offset
	.string	"main"                  // External Name
	.word	80                      // DIE offset
	.string	"bar"                   // External Name
	.word	0                       // End Mark
.LpubNames_end0:
	.section	.debug_pubtypes,"",@progbits
	.word	.LpubTypes_end0-.LpubTypes_begin0 // Length of Public Types Info
.LpubTypes_begin0:
	.half	2                       // DWARF Version
	.word	.Lcu_begin0             // Offset of Compilation Unit Info
	.word	109                     // Compilation Unit Length
	.word	101                     // DIE offset
	.string	"int"                   // External Name
	.word	0                       // End Mark
.LpubTypes_end0:
	.cfi_sections .debug_frame

	.ident	"QuIC LLVM Hexagon Clang version hexagon-clang-81-3364 (based on LLVM 4.0.0)"
	.section	".note.GNU-stack","",@progbits
	.section	.debug_line,"",@progbits
.Lline_table_start0:
