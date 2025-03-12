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
	.section	.text.foo,"ax",%progbits
	.globl	foo
	.p2align	2
	.type	foo,%function
	.code	32                      @ @foo
foo:
	.fnstart
@ BB#0:                                 @ %entry
	mov	r0, #0
	bx	lr
.Lfunc_end0:
	.size	foo, .Lfunc_end0-foo
	.cantunwind
	.fnend

	.section	.text.bar,"ax",%progbits
	.globl	bar
	.p2align	2
	.type	bar,%function
	.code	32                      @ @bar
bar:
	.fnstart
@ BB#0:                                 @ %entry
	mov	r0, #0
	bx	lr
.Lfunc_end1:
	.size	bar, .Lfunc_end1-bar
	.cantunwind
	.fnend


	.ident	"Snapdragon LLVM ARM Compiler 4.0.0 (based on LLVM 4.0.0)"
	.section	".note.GNU-stack","",%progbits
	.eabi_attribute	30, 5	@ Tag_ABI_optimization_goals
