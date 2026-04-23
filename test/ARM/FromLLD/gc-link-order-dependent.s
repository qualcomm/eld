
// RUN: llvm-mc -filetype=obj -triple=armv7a-none-linux-gnueabi %s -o %t.o
// RUN: %link --gc-sections --print-gc-sections -e _start %t.o -o %t.out 2>&1 | %filecheck %s --implicit-check-not=.linkorder.foo

// SHF_LINK_ORDER sections depend on their linked sections (sh_link).
// When --gc-sections keeps .text.foo, it should also keep a dependent section
// marked with SHF_LINK_ORDER and sh_link=.text.foo.

// CHECK-DAG: GC : {{.*}}[.text.bar]
// CHECK-DAG: GC : {{.*}}[.linkorder.bar]

  .syntax unified

  .section .text.start,"ax",%progbits
  .globl _start
_start:
  bl foo
  bx lr

  .section .text.foo,"ax",%progbits
  .globl foo
foo:
  bx lr

  .section .text.bar,"ax",%progbits
  .globl bar
bar:
  bx lr

  .section .linkorder.foo,"ao",%progbits,.text.foo
  .word 0
  .word 1

  .section .linkorder.bar,"ao",%progbits,.text.bar
  .word 0
  .word 1
