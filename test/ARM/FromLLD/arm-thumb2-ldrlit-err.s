// REQUIRES: arm
// Ported from: https://github.com/llvm/llvm-project/blob/main/lld/test/ELF/arm-thumb2-ldrlit-err.s
// RUN: llvm-mc --triple=thumbv7m-none-eabi --arm-add-build-attributes -filetype=obj -o %t.o %s
// RUN: %not %link %t.o -o %t 2>&1 | %filecheck %s

 .section .text.0, "ax", %progbits
 .thumb_func
 .balign 4
low:
  bx lr
  nop
  nop

 .section .text.1, "ax", %progbits
 .global _start
 .thumb_func
_start:
// CHECK: Error: {{.*}}(.text.1{{.*}}): relocation R_ARM_THM_PC12 out of range
 .inst.w 0xf85f0fff
 .reloc 0, R_ARM_THM_PC12, low
// CHECK: Error: {{.*}}(.text.1+0x4): relocation R_ARM_THM_PC12 out of range
 .inst.w 0xf8df0ff7
 .reloc 4, R_ARM_THM_PC12, high

 .section .text.2
 .thumb_func
 .balign 4
high:
 bx lr
