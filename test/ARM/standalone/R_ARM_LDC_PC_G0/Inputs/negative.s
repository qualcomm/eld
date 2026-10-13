.syntax unified
.arm

.text
.balign 4

.type target, %object
target:
    .word 42

    // Place load_target exactly 0x100 bytes after target.
    .space 0xfc

load_target:
    // U=1 and imm8=0 gives the implicit addend A = 0.
    //
    // S = target      = 0x0
    // P = load_target = 0x100
    //
    // X = S + A - P
    //   = 0 - 0x100
    //   = -0x100
    //
    // The relocation must clear U and encode:
    // imm8 = 0x100 >> 2 = 0x40.
    .inst 0xed9f0100
    .reloc load_target, R_ARM_LDC_PC_G0, target

    bx lr
