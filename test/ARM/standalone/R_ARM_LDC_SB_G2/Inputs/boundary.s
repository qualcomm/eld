.syntax unified
.arm

.section .text,"ax",%progbits
.balign 4

load_target:
    // U=1, imm8=0 gives A = 0.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_SB_G2, target

.section .bss,"aw",%nobits
    // B(S) = 0x1000.
    //
    // X = 0x40403fc.
    //
    // G2 residual = 0x3fc, the maximum valid aligned residual.
    .space 0x40403fc

.type target, %object
target:
    .space 4
