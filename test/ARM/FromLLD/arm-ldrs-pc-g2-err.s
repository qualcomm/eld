// REQUIRES: arm
// Ported from: lld/test/ELF/arm-adr-err-long.s
// RUN: %llvm-mc --triple=armv7a-none-eabi --arm-add-build-attributes -filetype=obj -o %t.o %s
// RUN: %not %link %t.o -T %p/Inputs/arm-ldrs-pc-g2-err.lds -o %t 2>&1 | %filecheck %s

.syntax unified
.arm

.section .text.1, "ax", %progbits

// Preserve the offsets from the original LLD test.
.space 32

.inst 0xe24f0008 // sub r0, pc, #8
.inst 0xe2400004 // sub r0, r0, #4
.inst 0xe1c000d0 // ldrd r0, r1, [r0, #0]

.reloc 32, R_ARM_ALU_PC_G0_NC, dat2
.reloc 36, R_ARM_ALU_PC_G1_NC, dat2

// G2 residual is 4056 (0xfd8), which does not fit in 8 bits.
// CHECK: Error: {{.*}}R_ARM_LDRS_PC_G2 out of range: 4056 is not in [0, 255]
.reloc 40, R_ARM_LDRS_PC_G2, dat2

.section .text.2, "ax", %progbits
dat2:
.word 0
