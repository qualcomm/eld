        .text
        .globl h
  h:
        leaq _GLOBAL_OFFSET_TABLE_(%rip), %rbx
        ret
