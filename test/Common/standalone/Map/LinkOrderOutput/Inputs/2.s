#include "asm_section_type_macros.h"

  .section .text.bar,"ax",SECTYPE_TOKEN
  .globl bar
  .type bar, OBJTYPE_TOKEN
bar:
  .word 0

  .section .linkorder,"ao",SECTYPE_TOKEN,.text.bar
  .word 0x22
