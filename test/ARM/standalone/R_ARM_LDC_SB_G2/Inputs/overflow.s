.syntax unified
.arm

.section .text,"ax",%progbits
.balign 4

load_target:
    // U=1, imm8=0 gives A = 0.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_SB_G2, target

.section .bss,"aw",%nobits
    // X = 0x4040400.
    // G2 residual = 0x400, which is outside [0, 0x3ff].
    .space 0x4040400

.type target, %object
target:
    .space 4
