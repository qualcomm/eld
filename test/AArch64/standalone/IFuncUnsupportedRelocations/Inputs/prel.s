  .section .data,"aw",%progbits
  .global foo_gp
  foo_gp:
    .quad foo - .
