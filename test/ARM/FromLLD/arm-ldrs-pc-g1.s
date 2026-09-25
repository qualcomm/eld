// REQUIRES: arm
// Ported from: lld/test/ELF/arm-adr-long.s
// RUN: llvm-mc --triple=armv7a-none-eabi --arm-add-build-attributes -filetype=obj -o %t.o %s
// RUN: %link %t.o -T %p/Inputs/arm-ldrs-pc-g1.lds -o %t
// RUN: %readelf -x .text.2 %t | %filecheck %s

.syntax unified
.arm

.section .text.1, "ax", %progbits
dat1:
  .word 0

.section .text.2, "ax", %progbits
.space 0x40

.global _start
.type _start, %function
_start:
  .inst 0xe24f0008 // sub r0, pc, #8
  .inst 0xe14000d4 // ldrd r0, [r0, #-4]

.reloc 64, R_ARM_ALU_PC_G0_NC, dat1
.reloc 68, R_ARM_LDRS_PC_G1, dat1

// R_ARM_ALU_PC_G0_NC encodes the first group as -0x700000.
// R_ARM_LDRS_PC_G1 encodes the remaining offset as -72.
// CHECK: 0x00800040 70084fe2 d80440e1
