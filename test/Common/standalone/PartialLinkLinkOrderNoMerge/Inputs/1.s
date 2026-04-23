#include "asm_section_type_macros.h"

  .section .text,"ax",SECTYPE_TOKEN
  .globl f1
  .type f1, OBJTYPE_TOKEN
f1:
  .word 0

  .section .linkorder,"ao",SECTYPE_TOKEN,.text
  .word 1
