 .cpu cortex-a8
 .eabi_attribute Tag_CPU_arch, 10
 .section .thumb_caller, "ax", %progbits
 .balign 0x100
 .thumb
 .globl thumb_caller
 .type thumb_caller, %function
thumb_caller:
 b.w arm_callee1
