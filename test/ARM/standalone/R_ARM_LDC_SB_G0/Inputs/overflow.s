.syntax unified
.arm

.section .text,"ax",%progbits
.balign 4

load_target:
    // U=1, imm8=0 gives A = 0.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_SB_G0, target

.section .data,"aw",%progbits
    // B(S) = 0x1000 and target = 0x2000.
    //
    // X = S + A - B(S)
    //   = 0x2000 - 0x1000
    //   = 0x1000
    //
    // The selected residual does not fit in the LDC/STC immediate.
    .space 0x1000

.type target, %object
target:
    .word 42
