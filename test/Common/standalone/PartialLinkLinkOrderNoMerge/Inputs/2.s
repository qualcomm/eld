#include "asm_section_type_macros.h"

  .section .text,"ax",SECTYPE_TOKEN
  .globl f2
  .type f2, OBJTYPE_TOKEN
f2:
  .word 0

  .section .linkorder,"ao",SECTYPE_TOKEN,.text
  .word 2
