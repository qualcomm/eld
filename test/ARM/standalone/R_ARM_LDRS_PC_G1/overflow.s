// REQUIRES: arm
// RUN: llvm-mc --triple=armv7a-none-eabi --arm-add-build-attributes -filetype=obj -o %t.o %s
// RUN: %not %link %t.o -o %t 2>&1 | %filecheck %s

.syntax unified
.arm

.text
.balign 4

.global _start
.type _start, %function
_start:
  .inst 0xe14f00d0
  .reloc _start, R_ARM_LDRS_PC_G1, target

// target - _start = 0x10100.
// G1 produces residual 0x100, which does not fit in 8 bits.
.space 0x100fc

.global target
target:
  .word 42

// CHECK: Error: {{.*}}R_ARM_LDRS_PC_G1{{.*}}out of range
