// REQUIRES: arm
// RUN: llvm-mc --triple=armv7a-none-eabi --arm-add-build-attributes -filetype=obj -o %t.o %s
// RUN: not %link %t.o -o /dev/null 2>&1 | %filecheck %s
 .section .os0, "ax", %progbits
 .balign 1024
 .thumb_func
low:
 bx lr

/// Check that we error when the immediate for the add or sub is not encodeable
 .section .os1, "ax", %progbits
 .arm
 .balign 1024
 .global _start
 .type _start, %function
_start:
// CHECK: Relocation overflow when applying relocation `R_ARM_ALU_PC_G0' for symbol `low' referred from {{.*}}.s.tmp.o[.os1] symbol defined in {{.*}}.s.tmp.o[.os0]
/// adr r0, low
 .inst 0xe24f0008
 .reloc 0, R_ARM_ALU_PC_G0, low
// CHECK: Relocation overflow when applying relocation `R_ARM_ALU_PC_G0' for symbol `unaligned' referred from {{.*}}.s.tmp.o[.os1] symbol defined in {{.*}}.s.tmp.o[.os2]
/// adr r1, unaligned
 .inst 0xe24f1008
 .reloc 4, R_ARM_ALU_PC_G0, unaligned

 .section .os2, "ax", %progbits
 .balign 1024
 .thumb_func
unaligned:
  bx lr
