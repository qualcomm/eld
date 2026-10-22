.syntax unified
.arm

.section .text,"ax",%progbits
.balign 4

load_target:
    // U=0, imm8=0x40 gives A = -0x100.
    .inst 0xed1f0140
    .reloc load_target, R_ARM_LDC_SB_G2, target

    bx lr

.section .bss,"aw",%nobits
    // B(S) = 0x1000 and target = 0x1080.
    //
    // X = 0x1080 - 0x100 - 0x1000
    //   = -0x80.
    //
    // G2 residual = 0.
    .space 0x80

.type target, %object
target:
    .space 4
