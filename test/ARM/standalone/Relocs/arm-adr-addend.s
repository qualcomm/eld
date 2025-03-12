// RUN: llvm-mc --triple=armv7a-none-eabi --arm-add-build-attributes -filetype=obj -o %t.o %s
// RUN: %link --image-base=0x20000 %t.o -o %t
// RUN: %objdump -d --triple=armv7a-none-eabi %t | %filecheck %s

 .arm
 .text
 .global _start
 .type _start, %function
_start:
 .reloc 0, R_ARM_ALU_PC_G0, .
 .inst 0xe24f0001
 .reloc 4, R_ARM_ALU_PC_G0, .
 .inst 0xe24f0101
 .reloc 8, R_ARM_ALU_PC_G0, .
 .inst 0xe24f0F01

 .reloc 12, R_ARM_ALU_PC_G0, .
 .inst 0xe28f0001
 .reloc 16, R_ARM_ALU_PC_G0, .
 .inst 0xe28f0101
 .reloc 20, R_ARM_ALU_PC_G0, .
 .inst 0xe28f0F01

// CHECK:      20000 <_start>:
// CHECK:      20000: e24f0001      sub r0, pc, #1
// CHECK:      20004: e24f0440      sub r0, pc, #64, #8
// CHECK:      20008: e24f0004      sub r0, pc, #4
// CHECK:      2000c: e28f0001      add r0, pc, #1
// CHECK:      20010: e28f0440      add r0, pc, #64, #8
// CHECK:      20014: e28f0004      add r0, pc, #4
