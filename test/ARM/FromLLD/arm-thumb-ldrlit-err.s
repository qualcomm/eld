// REQUIRES: arm
// Ported from: https://github.com/llvm/llvm-project/blob/main/lld/test/ELF/arm-thumb-ldrlit-err.s
// RUN: llvm-mc --triple=thumbv6m-none-eabi --arm-add-build-attributes -filetype=obj -o %t.o %s
// RUN: %not %link %t.o -o %t 2>&1 | %filecheck %s

 .section .text.0, "ax", %progbits
 .balign 4
 .thumb_func
low:
 bx lr

 .section .text.1, "ax", %progbits
 .balign 2
 .global _start
 .thumb_func
_start:
// CHECK: Error: {{.*}}(.text.1{{.*}}): relocation R_ARM_THM_PC8 out of range
 .inst.n 0x48ff
 .reloc 0, R_ARM_THM_PC8, low
// CHECK: Error: {{.*}}R_ARM_THM_PC8{{.*}}unaligned
 .inst.n 0x49ff
 .reloc 2, R_ARM_THM_PC8, unaligned
// CHECK: Error: {{.*}}(.text.1+0x4): relocation R_ARM_THM_PC8 out of range
 .inst.n 0x4aff
 .reloc 4, R_ARM_THM_PC8, range

 .section .text.2, "ax", %progbits
 .balign 4
 nop
 .thumb_func
unaligned:
  bx lr
 .space 1024
range:
  bx lr
