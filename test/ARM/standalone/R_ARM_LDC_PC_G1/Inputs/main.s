.syntax unified
.arm

.text
.balign 4

load_target:
    // U=1 and imm8=0 gives A = 0.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_PC_G1, target

    bx lr

    // target - load_target = 0x1234.
    .space 0x122c

.type target, %object
target:
    .word 42
