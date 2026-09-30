.syntax unified
.arm

.text
.balign 4

load_target:
    // LDC with U=1 and imm8=0, so the implicit addend is A = 0.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_PC_G0, target

    bx lr

    // target - load_target = 0x100.
    .space 0xf8

.balign 4
.type target, %object
target:
    .word 42
