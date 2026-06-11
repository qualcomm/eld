SECTIONS {
  .foo    : { *(.text.foo) }
  .tbss.a : { *(.tbss.tls_a) }
  .empty  : {}
  .tbss.b : { *(.tbss.tls_b) }
  .tbss.c : { *(.tbss.tls_c) }
  .data   : { *(.data.data) }
}
