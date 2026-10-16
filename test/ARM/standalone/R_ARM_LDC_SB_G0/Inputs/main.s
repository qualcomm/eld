.syntax unified
.arm

.section .text,"ax",%progbits
.balign 4

load_target:
    // U=1, imm8=0 gives the implicit addend A = 0.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_SB_G0, target

    bx lr

.section .data,"aw",%progbits
    // B(S) = 0x1000 and target = 0x1100.
    //
    // X = S + A - B(S)
    //   = 0x1100 - 0x1000
    //   = 0x100
    //
    // imm8 = 0x100 >> 2 = 0x40.
    .space 0x100

.type target, %object
target:
    .word 42
