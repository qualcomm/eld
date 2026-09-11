// REQUIRES: arm
// Ported from: https://github.com/llvm/llvm-project/blob/main/lld/test/ELF/arm-thumb-ldrlit.s
// RUN: llvm-mc --triple=thumbv6m-none-eabi --arm-add-build-attributes -filetype=obj -o %t.o %s
// RUN: echo "SECTIONS { \
// RUN:   .text.01 0x1000 : { *(.text.01) } \
// RUN:   .text.02 0x1004 : { *(.text.02) } \
// RUN: } " > %t.script
// RUN: %link -n --script %t.script %t.o -o %t
// RUN: %readelf -x .text.01 %t | %filecheck %s

/// Test R_ARM_THM_PC8: P=0x1000, target=0x1004, val=4, imm8=1
 .section .text.01, "ax", %progbits
 .balign 4
 .global _start
_start:
 .hword 0x0000
 .reloc 0, R_ARM_THM_PC8, target

 .section .text.02, "ax", %progbits
 .balign 4
 .global target
target:
 .word 42

// P=0x1000, target=0x1004, val=4, imm8=1 → 0x0001
// CHECK: 0100
