// RUN: %clang %clangopts -x assembler-with-cpp -I %standalone_inputs -c %s -o %t.o
// RUN: %link %linkopts --gc-sections --trace=link-order -e _start %t.o -o %t.out 2>&1 | %filecheck %s

// CHECK: LINK_ORDER : .linkorder.foo depends on .text.foo

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

  .section .linkorder.foo,"ao",SECTYPE_TOKEN,.text.foo
  .word 0x11
