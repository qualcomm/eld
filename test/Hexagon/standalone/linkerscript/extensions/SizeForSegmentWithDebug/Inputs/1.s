	.text
	.file	"1.c"
	.file	1 "1.c"
	.type	b,@object               // @b
	.section	.bss,"aw",@nobits
	.globl	b
	.p2align	2
b:
	.word	0                       // 0x0
	.size	b, 4

	.type	a,@object               // @a
	.data
	.globl	a
	.p2align	2
a:
	.word	10                      // 0xa
	.size	a, 4

	.type	c,@object               // @c
	.section	.bss,"aw",@nobits
	.globl	c
	.p2align	2
c:
	.word	0                       // 0x0
	.size	c, 4

	.section	.debug_str,"MS",@progbits,1
.Linfo_string0:
	.string	"QuIC LLVM Hexagon Clang version hexagon-clang-82-4057 (based on LLVM 5.0.0)" // string offset=0
.Linfo_string1:
	.string	"1.c"                   // string offset=76
.Linfo_string2:
	.string	"/local/mnt/workspace/shankare/work/llvmtip/mclinker_master/tools/mclinker/test/Hexagon/standalone/linkerscript/extensions/SizeForSegmentWithDebug/Inputs" // string offset=80
.Linfo_string3:
	.string	"b"                     // string offset=233
.Linfo_string4:
	.string	"int"                   // string offset=235
.Linfo_string5:
	.string	"a"                     // string offset=239
.Linfo_string6:
	.string	"c"                     // string offset=241
	.section	.debug_loc,"",@progbits
	.section	.debug_abbrev,"",@progbits
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
	.ascii	"\264B"                 // DW_AT_GNU_pubnames
	.byte	25                      // DW_FORM_flag_present
	.byte	0                       // EOM(1)
	.byte	0                       // EOM(2)
	.byte	2                       // Abbreviation Code
	.byte	52                      // DW_TAG_variable
	.byte	0                       // DW_CHILDREN_no
	.byte	3                       // DW_AT_name
	.byte	14                      // DW_FORM_strp
	.byte	73                      // DW_AT_type
	.byte	19                      // DW_FORM_ref4
	.byte	63                      // DW_AT_external
	.byte	25                      // DW_FORM_flag_present
	.byte	58                      // DW_AT_decl_file
	.byte	11                      // DW_FORM_data1
	.byte	59                      // DW_AT_decl_line
	.byte	11                      // DW_FORM_data1
	.byte	2                       // DW_AT_location
	.byte	24                      // DW_FORM_exprloc
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
.Lcu_begin0:
	.word	85                      // Length of Unit
	.half	4                       // DWARF version number
	.word	.debug_abbrev           // Offset Into Abbrev. Section
	.byte	4                       // Address Size (in bytes)
	.byte	1                       // Abbrev [1] 0xb:0x4e DW_TAG_compile_unit
	.word	.Linfo_string0          // DW_AT_producer
	.half	12                      // DW_AT_language
	.word	.Linfo_string1          // DW_AT_name
	.word	.Lline_table_start0     // DW_AT_stmt_list
	.word	.Linfo_string2          // DW_AT_comp_dir
                                        // DW_AT_GNU_pubnames
	.byte	2                       // Abbrev [2] 0x1e:0x11 DW_TAG_variable
	.word	.Linfo_string3          // DW_AT_name
	.word	47                      // DW_AT_type
                                        // DW_AT_external
	.byte	1                       // DW_AT_decl_file
	.byte	1                       // DW_AT_decl_line
	.byte	5                       // DW_AT_location
	.byte	3
	.word	b
	.byte	3                       // Abbrev [3] 0x2f:0x7 DW_TAG_base_type
	.word	.Linfo_string4          // DW_AT_name
	.byte	5                       // DW_AT_encoding
	.byte	4                       // DW_AT_byte_size
	.byte	2                       // Abbrev [2] 0x36:0x11 DW_TAG_variable
	.word	.Linfo_string5          // DW_AT_name
	.word	47                      // DW_AT_type
                                        // DW_AT_external
	.byte	1                       // DW_AT_decl_file
	.byte	2                       // DW_AT_decl_line
	.byte	5                       // DW_AT_location
	.byte	3
	.word	a
	.byte	2                       // Abbrev [2] 0x47:0x11 DW_TAG_variable
	.word	.Linfo_string6          // DW_AT_name
	.word	47                      // DW_AT_type
                                        // DW_AT_external
	.byte	1                       // DW_AT_decl_file
	.byte	3                       // DW_AT_decl_line
	.byte	5                       // DW_AT_location
	.byte	3
	.word	c
	.byte	0                       // End Of Children Mark
	.section	.debug_ranges,"",@progbits
	.section	.debug_macinfo,"",@progbits
.Lcu_macro_begin0:
	.byte	0                       // End Of Macro List Mark
	.section	.debug_pubnames,"",@progbits
	.word	.LpubNames_end0-.LpubNames_begin0 // Length of Public Names Info
.LpubNames_begin0:
	.half	2                       // DWARF Version
	.word	.Lcu_begin0             // Offset of Compilation Unit Info
	.word	89                      // Compilation Unit Length
	.word	54                      // DIE offset
	.string	"a"                     // External Name
	.word	30                      // DIE offset
	.string	"b"                     // External Name
	.word	71                      // DIE offset
	.string	"c"                     // External Name
	.word	0                       // End Mark
.LpubNames_end0:
	.section	.debug_pubtypes,"",@progbits
	.word	.LpubTypes_end0-.LpubTypes_begin0 // Length of Public Types Info
.LpubTypes_begin0:
	.half	2                       // DWARF Version
	.word	.Lcu_begin0             // Offset of Compilation Unit Info
	.word	89                      // Compilation Unit Length
	.word	47                      // DIE offset
	.string	"int"                   // External Name
	.word	0                       // End Mark
.LpubTypes_end0:

	.ident	"QuIC LLVM Hexagon Clang version hexagon-clang-82-4057 (based on LLVM 5.0.0)"
	.section	".note.GNU-stack","",@progbits
	.section	.debug_line,"",@progbits
.Lline_table_start0:
