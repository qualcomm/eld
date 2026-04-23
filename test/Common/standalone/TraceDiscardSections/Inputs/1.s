#include "asm_section_type_macros.h"

  .section .text.start,"ax",SECTYPE_TOKEN
  .globl _start
  .type _start, OBJTYPE_TOKEN
_start:
  .word 0

  .section .text.dep,"ax",SECTYPE_TOKEN
  .globl dep
  .type dep, OBJTYPE_TOKEN
dep:
  .word 0x1234

  .section .linkorder.dep,"ao",SECTYPE_TOKEN,.text.dep
  .word 0x1111
