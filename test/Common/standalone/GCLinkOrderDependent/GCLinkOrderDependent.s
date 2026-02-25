// REQUIRES: arm
// RUN: %clang %clangopts -x assembler-with-cpp -I %standalone_inputs -c %s -o %t.o
// RUN: %link %linkopts --gc-sections --print-gc-sections -e _start %t.o -o %t.out 2>&1 | %filecheck %s --implicit-check-not=.linkorder.foo

// SHF_LINK_ORDER sections depend on their linked sections (sh_link).
// When --gc-sections collects .text.bar, it should also collect the dependent
// .linkorder.bar section. A kept section's dependent (.linkorder.foo) should
// not be collected.

// CHECK-DAG: GC : {{.*}}[.text.bar]
// CHECK-DAG: GC : {{.*}}[.linkorder.bar]

  .syntax unified

#include "asm_section_type_macros.h"

  .section .text.start,"ax",SECTYPE_TOKEN
  .globl _start
  .type _start, OBJTYPE_TOKEN
_start:
  .word foo

  .section .text.foo,"ax",SECTYPE_TOKEN
  .globl foo
  .type foo, OBJTYPE_TOKEN
foo:
  .word 0

  .section .text.bar,"ax",SECTYPE_TOKEN
  .globl bar
  .type bar, OBJTYPE_TOKEN
bar:
  .word 0

  .section .linkorder.foo,"ao",SECTYPE_TOKEN,.text.foo
  .word 0
  .word 1

  .section .linkorder.bar,"ao",SECTYPE_TOKEN,.text.bar
  .word 0
  .word 1
