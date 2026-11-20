.syntax unified
.arm

.section .text,"ax",%progbits
.balign 4

load_target:
    // U=1, imm8=0 gives A = 0.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_SB_G1, target

    bx lr

.section .data,"aw",%progbits
    // B(S) = 0x1000 and target = 0x1404.
    //
    // X = S + A - B(S)
    //   = 0x404.
    //
    // G1 residual = 0x4, so imm8 = 1.
    .space 0x404

.type target, %object
target:
    .word 42
