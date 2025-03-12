	.text
	.syntax unified
	.eabi_attribute	67, "2.09"	@ Tag_conformance
	.cpu	cortex-a9
	.eabi_attribute	6, 10	@ Tag_CPU_arch
	.eabi_attribute	7, 65	@ Tag_CPU_arch_profile
	.eabi_attribute	8, 1	@ Tag_ARM_ISA_use
	.eabi_attribute	9, 2	@ Tag_THUMB_ISA_use
	.arch_extension	idiv
	.eabi_attribute	17, 1	@ Tag_ABI_PCS_GOT_use
	.eabi_attribute	20, 1	@ Tag_ABI_FP_denormal
	.eabi_attribute	21, 1	@ Tag_ABI_FP_exceptions
	.eabi_attribute	23, 3	@ Tag_ABI_FP_number_model
	.eabi_attribute	34, 0	@ Tag_CPU_unaligned_access
	.eabi_attribute	24, 1	@ Tag_ABI_align_needed
	.eabi_attribute	25, 1	@ Tag_ABI_align_preserved
	.eabi_attribute	36, 1	@ Tag_FP_HP_extension
	.eabi_attribute	38, 1	@ Tag_ABI_FP_16bit_format
	.eabi_attribute	18, 4	@ Tag_ABI_PCS_wchar_t
	.eabi_attribute	26, 2	@ Tag_ABI_enum_size
	.eabi_attribute	14, 0	@ Tag_ABI_PCS_R9_use
	.eabi_attribute	68, 1	@ Tag_Virtualization_use
	.text
	.globl	mon_smc_handler
	.align	2
	.type	mon_smc_handler,%function
	.code	16                      @ @mon_smc_handler
	.thumb_func
mon_smc_handler:
	movw	r11, :lower16:g_mon_did_irq_exit
	movt	r11, :upper16:g_mon_did_irq_exit

	.type	g_mon_did_irq_exit,%object @ @g_mon_did_irq_exit
	.globl	g_mon_did_irq_exit
g_mon_did_irq_exit:
	.byte	0                       @ 0x0
	.size	g_mon_did_irq_exit, 1

