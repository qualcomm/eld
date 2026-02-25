#include "asm_section_type_macros.h"

  .section .text.foo,"ax",SECTYPE_TOKEN
  .globl foo
  .type foo, OBJTYPE_TOKEN
foo:
  .word 0

  .section .linkorder,"ao",SECTYPE_TOKEN,.text.foo
  .word 0x11
