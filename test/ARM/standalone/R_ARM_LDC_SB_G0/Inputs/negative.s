.syntax unified
.arm

.section .text,"ax",%progbits
.balign 4

load_target:
    // U=0, imm8=0x40 gives the implicit addend A = -0x100.
    .inst 0xed1f0140
    .reloc load_target, R_ARM_LDC_SB_G0, target

    bx lr

.section .data,"aw",%progbits
    // B(S) = 0x1000 and target = 0x1080.
    //
    // X = S + A - B(S)
    //   = 0x1080 - 0x100 - 0x1000
    //   = -0x80
    //
    // U = 0 and imm8 = 0x80 >> 2 = 0x20.
    .space 0x80

.type target, %object
target:
    .word 42
