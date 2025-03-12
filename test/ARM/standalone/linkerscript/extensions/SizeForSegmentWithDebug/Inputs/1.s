	.text
	.syntax unified
	.eabi_attribute	67, "2.09"	@ Tag_conformance
	.cpu	arm7tdmi
	.eabi_attribute	6, 2	@ Tag_CPU_arch
	.eabi_attribute	8, 1	@ Tag_ARM_ISA_use
	.eabi_attribute	17, 1	@ Tag_ABI_PCS_GOT_use
	.eabi_attribute	20, 2	@ Tag_ABI_FP_denormal
	.eabi_attribute	21, 0	@ Tag_ABI_FP_exceptions
	.eabi_attribute	23, 3	@ Tag_ABI_FP_number_model
	.eabi_attribute	34, 0	@ Tag_CPU_unaligned_access
	.eabi_attribute	24, 1	@ Tag_ABI_align_needed
	.eabi_attribute	25, 1	@ Tag_ABI_align_preserved
	.eabi_attribute	38, 1	@ Tag_ABI_FP_16bit_format
	.eabi_attribute	18, 4	@ Tag_ABI_PCS_wchar_t
	.eabi_attribute	26, 2	@ Tag_ABI_enum_size
	.eabi_attribute	14, 0	@ Tag_ABI_PCS_R9_use
	.file	"1.c"
	.file	1 "1.c"
	.type	b,%object               @ @b
	.bss
	.globl	b
	.p2align	2
b:
	.long	0                       @ 0x0
	.size	b, 4

	.type	a,%object               @ @a
	.data
	.globl	a
	.p2align	2
a:
	.long	10                      @ 0xa
	.size	a, 4

	.type	c,%object               @ @c
	.bss
	.globl	c
	.p2align	2
c:
	.long	0                       @ 0x0
	.size	c, 4

	.section	.debug_str,"MS",%progbits,1
.Linfo_string0:
	.asciz	"Snapdragon LLVM ARM Compiler 4.0.0 (based on LLVM 4.0.0)" @ string offset=0
.Linfo_string1:
	.asciz	"1.c"                   @ string offset=57
.Linfo_string2:
	.asciz	"/usr2/seaswara/work/llvmtip/mclinker_master/tools/mclinker/test/ARM/standalone/linkerscript/extensions/SizeForSegmentWithDebug/Inputs" @ string offset=61
.Linfo_string3:
	.asciz	"b"                     @ string offset=195
.Linfo_string4:
	.asciz	"int"                   @ string offset=197
.Linfo_string5:
	.asciz	"a"                     @ string offset=201
.Linfo_string6:
	.asciz	"c"                     @ string offset=203
	.section	.debug_loc,"",%progbits
	.section	.debug_abbrev,"",%progbits
	.byte	1                       @ Abbreviation Code
	.byte	17                      @ DW_TAG_compile_unit
	.byte	1                       @ DW_CHILDREN_yes
	.byte	37                      @ DW_AT_producer
	.byte	14                      @ DW_FORM_strp
	.byte	19                      @ DW_AT_language
	.byte	5                       @ DW_FORM_data2
	.byte	3                       @ DW_AT_name
	.byte	14                      @ DW_FORM_strp
	.byte	16                      @ DW_AT_stmt_list
	.byte	23                      @ DW_FORM_sec_offset
	.byte	27                      @ DW_AT_comp_dir
	.byte	14                      @ DW_FORM_strp
	.byte	0                       @ EOM(1)
	.byte	0                       @ EOM(2)
	.byte	2                       @ Abbreviation Code
	.byte	52                      @ DW_TAG_variable
	.byte	0                       @ DW_CHILDREN_no
	.byte	3                       @ DW_AT_name
	.byte	14                      @ DW_FORM_strp
	.byte	73                      @ DW_AT_type
	.byte	19                      @ DW_FORM_ref4
	.byte	63                      @ DW_AT_external
	.byte	25                      @ DW_FORM_flag_present
	.byte	58                      @ DW_AT_decl_file
	.byte	11                      @ DW_FORM_data1
	.byte	59                      @ DW_AT_decl_line
	.byte	11                      @ DW_FORM_data1
	.byte	2                       @ DW_AT_location
	.byte	24                      @ DW_FORM_exprloc
	.byte	0                       @ EOM(1)
	.byte	0                       @ EOM(2)
	.byte	3                       @ Abbreviation Code
	.byte	36                      @ DW_TAG_base_type
	.byte	0                       @ DW_CHILDREN_no
	.byte	3                       @ DW_AT_name
	.byte	14                      @ DW_FORM_strp
	.byte	62                      @ DW_AT_encoding
	.byte	11                      @ DW_FORM_data1
	.byte	11                      @ DW_AT_byte_size
	.byte	11                      @ DW_FORM_data1
	.byte	0                       @ EOM(1)
	.byte	0                       @ EOM(2)
	.byte	0                       @ EOM(3)
	.section	.debug_info,"",%progbits
.Lcu_begin0:
	.long	85                      @ Length of Unit
	.short	4                       @ DWARF version number
	.long	.debug_abbrev           @ Offset Into Abbrev. Section
	.byte	4                       @ Address Size (in bytes)
	.byte	1                       @ Abbrev [1] 0xb:0x4e DW_TAG_compile_unit
	.long	.Linfo_string0          @ DW_AT_producer
	.short	12                      @ DW_AT_language
	.long	.Linfo_string1          @ DW_AT_name
	.long	.Lline_table_start0     @ DW_AT_stmt_list
	.long	.Linfo_string2          @ DW_AT_comp_dir
	.byte	2                       @ Abbrev [2] 0x1e:0x11 DW_TAG_variable
	.long	.Linfo_string3          @ DW_AT_name
	.long	47                      @ DW_AT_type
                                        @ DW_AT_external
	.byte	1                       @ DW_AT_decl_file
	.byte	1                       @ DW_AT_decl_line
	.byte	5                       @ DW_AT_location
	.byte	3
	.long	b
	.byte	3                       @ Abbrev [3] 0x2f:0x7 DW_TAG_base_type
	.long	.Linfo_string4          @ DW_AT_name
	.byte	5                       @ DW_AT_encoding
	.byte	4                       @ DW_AT_byte_size
	.byte	2                       @ Abbrev [2] 0x36:0x11 DW_TAG_variable
	.long	.Linfo_string5          @ DW_AT_name
	.long	47                      @ DW_AT_type
                                        @ DW_AT_external
	.byte	1                       @ DW_AT_decl_file
	.byte	2                       @ DW_AT_decl_line
	.byte	5                       @ DW_AT_location
	.byte	3
	.long	a
	.byte	2                       @ Abbrev [2] 0x47:0x11 DW_TAG_variable
	.long	.Linfo_string6          @ DW_AT_name
	.long	47                      @ DW_AT_type
                                        @ DW_AT_external
	.byte	1                       @ DW_AT_decl_file
	.byte	3                       @ DW_AT_decl_line
	.byte	5                       @ DW_AT_location
	.byte	3
	.long	c
	.byte	0                       @ End Of Children Mark
	.section	.debug_ranges,"",%progbits
	.section	.debug_macinfo,"",%progbits
.Lcu_macro_begin0:
	.byte	0                       @ End Of Macro List Mark
	.section	.debug_pubnames,"",%progbits
	.long	.LpubNames_end0-.LpubNames_begin0 @ Length of Public Names Info
.LpubNames_begin0:
	.short	2                       @ DWARF Version
	.long	.Lcu_begin0             @ Offset of Compilation Unit Info
	.long	89                      @ Compilation Unit Length
	.long	54                      @ DIE offset
	.asciz	"a"                     @ External Name
	.long	30                      @ DIE offset
	.asciz	"b"                     @ External Name
	.long	71                      @ DIE offset
	.asciz	"c"                     @ External Name
	.long	0                       @ End Mark
.LpubNames_end0:
	.section	.debug_pubtypes,"",%progbits
	.long	.LpubTypes_end0-.LpubTypes_begin0 @ Length of Public Types Info
.LpubTypes_begin0:
	.short	2                       @ DWARF Version
	.long	.Lcu_begin0             @ Offset of Compilation Unit Info
	.long	89                      @ Compilation Unit Length
	.long	47                      @ DIE offset
	.asciz	"int"                   @ External Name
	.long	0                       @ End Mark
.LpubTypes_end0:

	.ident	"Snapdragon LLVM ARM Compiler 4.0.0 (based on LLVM 4.0.0)"
	.section	".note.GNU-stack","",%progbits
	.section	.debug_line,"",%progbits
.Lline_table_start0:
