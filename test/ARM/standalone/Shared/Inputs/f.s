	.text
	.syntax unified
	.eabi_attribute	67, "2.09"	@ Tag_conformance
	.eabi_attribute	6, 2	@ Tag_CPU_arch
	.eabi_attribute	8, 1	@ Tag_ARM_ISA_use
	.eabi_attribute	15, 1	@ Tag_ABI_PCS_RW_data
	.eabi_attribute	16, 1	@ Tag_ABI_PCS_RO_data
	.eabi_attribute	17, 2	@ Tag_ABI_PCS_GOT_use
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
	.file	"f.bc"
	.globl	bar
	.align	2
	.type	bar,%function
bar:                                    @ @bar
	.fnstart
.Leh_func_begin0:
@ BB#0:                                 @ %entry
	.save	{r11, lr}
	push	{r11, lr}
	.setfp	r11, sp
	mov	r11, sp
	ldr	r0, .LCPI0_1
.LPC0_0:
	add	r0, pc, r0
	ldr	r1, .LCPI0_0
	ldr	r0, [r1, r0]
	blx	r0
	pop	{r11, lr}
	bx	lr
	.align	2
@ BB#1:
.LCPI0_0:
	.long	foo(GOT)
.LCPI0_1:
	.long	_GLOBAL_OFFSET_TABLE_-(.LPC0_0+8)
.Ltmp0:
	.size	bar, .Ltmp0-bar
	.cantunwind
	.fnend


	.ident	"Snapdragon LLVM ARM Compiler 3.6 (Yin's Special Build) for General Purpose Computing (based on LLVM 3.7.0)"
	.section	".note.GNU-stack","",%progbits
