// REQUIRES: arm
// RUN: split-file %s %t
// RUN: llvm-mc --triple=armv7a-none-eabi --arm-add-build-attributes -filetype=obj -o %t.o %t/asm
// RUN: not %link --script %t/lds %t.o -o /dev/null 2>&1 | %filecheck %s

// FIXME: Only R_ARM_ALU_PC_G0 is enabled.

//--- lds
SECTIONS {
    .text.0 0x0100000 : AT(0x0100000) { *(.text.0) }
    .text.1 0x0800000 : AT(0x0800000) { *(.text.1) }
    .text.2 0xf0f0000 : AT(0xf0f0000) { *(.text.2) }
}

//--- asm
/// This is a variant of arm-adr-long.s with some _NC relocs changed into their
/// checking counterparts, to verify that out-of-range references are caught.

 .section .text.0, "ax", %progbits
dat1:
 .word 0

 .section .text.1, "ax", %progbits
 .global _start
 .type _start, %function
_start:
 .inst 0xe24f0008 // sub r0, pc, #8
 .inst 0xe2400004 // sub r0, r0, #4
 .reloc 0, R_ARM_ALU_PC_G0, dat1
// CHECK: Error: Relocation overflow when applying relocation `R_ARM_ALU_PC_G0' for symbol `dat1' referred from {{.*}}.s.tmp.o[.text.1] symbol defined in {{.*}}.s.tmp.o[.text.0]
// FIXME .reloc 4, R_ARM_ALU_PC_G1, dat1

 .inst 0xe24f1008 // sub r1, pc, #8
 .inst 0xe2411004 // sub r1, r1, #4
 .inst 0xe2411000 // sub r1, r1, #0
// FIXME .reloc 8, R_ARM_ALU_PC_G0_NC, dat2
// FIXME .reloc 12, R_ARM_ALU_PC_G1, dat2
// FIXME: {{.*}}.s.tmp.o:(.text.1+0xc): unencodeable immediate 244252656 for relocation R_ARM_ALU_PC_G1
// FIXME .reloc 16, R_ARM_ALU_PC_G2, dat2

 .inst 0xe24f0008 // sub r0, pc, #8
 .inst 0xe2400004 // sub r0, r0, #4
 .inst 0xe2400000 // sub r0, r0, #0
 .reloc 20, R_ARM_ALU_PC_G0, dat1
// CHECK: Error: Relocation overflow when applying relocation `R_ARM_ALU_PC_G0' for symbol `dat1' referred from {{.*}}.s.tmp.o[.text.1] symbol defined in {{.*}}.s.tmp.o[.text.0]
// FIXME .reloc 24, R_ARM_ALU_PC_G1, dat1
// FIXME .reloc 28, R_ARM_ALU_PC_G2, dat1

 .inst 0xe24f0008 // sub r0, pc, #8
 .inst 0xe2400004 // sub r0, r0, #4
 .inst 0xe1c000d0 // ldrd r0, r1, [r0, #0]
// FIXME .reloc 32, R_ARM_ALU_PC_G0_NC, dat2
// FIXME .reloc 36, R_ARM_ALU_PC_G1_NC, dat2
// FIXME: {{.*}}.s.tmp.o:(.text.1+0x28): relocation R_ARM_LDRS_PC_G2 out of range: 4056 is not in [0, 255]; references dat2
// FIXME .reloc 40, R_ARM_LDRS_PC_G2, dat2

 .section .text.2, "ax", %progbits
dat2:
 .word 0
