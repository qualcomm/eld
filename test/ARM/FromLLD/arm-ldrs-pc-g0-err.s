// REQUIRES: arm
// Ported from: lld/test/ELF/arm-adr-err.s
// RUN: llvm-mc --triple=armv7a-none-eabi --arm-add-build-attributes -filetype=obj -o %t.o %s
// RUN: %not %link %t.o -o %t 2>&1 | %filecheck %s

.section .os0, "ax", %progbits
.balign 1024
.thumb_func
low:
  bx lr

.section .os1, "ax", %progbits
.arm
.balign 1024

.global _start
.type _start, %function
_start:
  nop

.balign 512

// R_ARM_LDRS_PC_G0 can encode only an unsigned 8-bit magnitude.
// CHECK: Error: {{.*}}R_ARM_LDRS_PC_G0{{.*}}out of range
.reloc ., R_ARM_LDRS_PC_G0, _start
.inst 0xe14f00d0
