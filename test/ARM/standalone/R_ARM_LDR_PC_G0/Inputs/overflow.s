.syntax unified
.arm

.section .text.1, "ax", %progbits
.globl overflow_func
.type overflow_func, %function
overflow_func:
    .reloc 0, R_ARM_LDR_PC_G0, overflow_target
    .inst 0xe51f0008
    bx lr

.section .text.2, "ax", %progbits
overflow_target:
    .word 0
