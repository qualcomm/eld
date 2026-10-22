.syntax unified
.arm

.section .text,"ax",%progbits
.balign 4

load_target:
    // U=1, imm8=0 gives A = 0.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_SB_G1, target

.section .data,"aw",%progbits
    // X = 0x40400.
    // G1 residual = 0x400, which is outside [0, 0x3ff].
    .space 0x40400

.type target, %object
target:
    .word 42
