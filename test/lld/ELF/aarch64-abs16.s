// REQUIRES: aarch64
// RUN: llvm-mc -filetype=obj -triple=aarch64 %s -o %t.o
// RUN: llvm-mc -filetype=obj -triple=aarch64 %S/Inputs/abs255.s -o %t255.o
// RUN: llvm-mc -filetype=obj -triple=aarch64 %S/Inputs/abs256.s -o %t256.o
// RUN: llvm-mc -filetype=obj -triple=aarch64 %S/Inputs/abs257.s -o %t257.o

.globl _start
_start:
.data
  .hword foo + 0xfeff
  .hword foo - 0x8100

// RUN: %link %linkopts --section-start=.data=0x220158 --no-align-segments %t.o %t256.o -o %t
// RUN: llvm-objdump -s --section=.data %t | FileCheck %s --check-prefixes=CHECK,LE

// CHECK: Contents of section .data:
// 220158: S = 0x100, A = 0xfeff
//         S + A = 0xffff
// 22015c: S = 0x100, A = -0x8100
//         S + A = 0x8000
// LE-NEXT: 220158 ffff0080

// RUN: not %link %linkopts %t.o %t255.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=OVERFLOW1
// OVERFLOW1: Relocation overflow when applying relocation `R_AARCH64_ABS16' for symbol `foo' referred from {{.+}}.o[.data] symbol defined in {{.+}}.o

// RUN: not %link %linkopts %t.o %t257.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=OVERFLOW2
// OVERFLOW2: Relocation overflow when applying relocation `R_AARCH64_ABS16' for symbol `foo' referred from {{.+}}.o[.data] symbol defined in {{.+}}.o
