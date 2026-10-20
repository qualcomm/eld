.syntax unified
.arm

.section .text,"ax",%progbits
.balign 4

load_target:
    // U=1, imm8=0 gives A = 0.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_SB_G1, target

.section .data,"aw",%progbits
    // B(S) = 0x1000.
    //
    // X = S + A - B(S)
    //   = 0x403fc.
    //
    // G1 residual = 0x3fc, the maximum valid aligned residual.
    .space 0x403fc

.type target, %object
target:
    .word 42
