#include "asm_section_type_macros.h"

.section .text.foo,"axG",SECTYPE_TOKEN,sig,comdat
.globl foo
.type foo, OBJTYPE_TOKEN
foo:
  .word 0

.section .linkorder,"aoG",SECTYPE_TOKEN,.text.foo,sig,comdat
  .word 0x22
