.syntax unified
.arm

.section .text,"ax",%progbits
.balign 4

load_target:
    // U=1, imm8=0 gives A = 0.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_SB_G0, target

.section .data,"aw",%progbits
    // B(S) = 0x1000 and target = 0x13fc.
    //
    // X = S + A - B(S)
    //   = 0x3fc.
    //
    // This is the maximum valid 4-byte-aligned residual.
    .space 0x3fc

.type target, %object
target:
    .word 42
