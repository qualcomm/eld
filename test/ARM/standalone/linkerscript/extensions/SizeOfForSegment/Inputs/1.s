	.text
	.syntax unified
	.eabi_attribute	67, "2.09"	@ Tag_conformance
	.cpu	arm7tdmi
	.eabi_attribute	6, 2	@ Tag_CPU_arch
	.eabi_attribute	8, 1	@ Tag_ARM_ISA_use
	.eabi_attribute	17, 1	@ Tag_ABI_PCS_GOT_use
	.eabi_attribute	20, 1	@ Tag_ABI_FP_denormal
	.eabi_attribute	21, 1	@ Tag_ABI_FP_exceptions
	.eabi_attribute	23, 3	@ Tag_ABI_FP_number_model
	.eabi_attribute	34, 0	@ Tag_CPU_unaligned_access
	.eabi_attribute	24, 1	@ Tag_ABI_align_needed
	.eabi_attribute	25, 1	@ Tag_ABI_align_preserved
	.eabi_attribute	38, 1	@ Tag_ABI_FP_16bit_format
	.eabi_attribute	18, 4	@ Tag_ABI_PCS_wchar_t
	.eabi_attribute	26, 2	@ Tag_ABI_enum_size
	.eabi_attribute	14, 0	@ Tag_ABI_PCS_R9_use
	.file	"1.c"
	.type	b,%object               @ @b
	.section	.bss.b,"aw",%nobits
	.globl	b
	.p2align	2
b:
	.long	0                       @ 0x0
	.size	b, 4

	.type	a,%object               @ @a
	.section	.data.a,"aw",%progbits
	.globl	a
	.p2align	2
a:
	.long	10                      @ 0xa
	.size	a, 4

	.type	c,%object               @ @c
	.section	.bss.c,"aw",%nobits
	.globl	c
	.p2align	2
c:
	.long	0                       @ 0x0
	.size	c, 4


	.ident	"Snapdragon LLVM ARM Compiler 3.9beta (based on llvm.org 3.9)"
	.section	".note.GNU-stack","",%progbits
