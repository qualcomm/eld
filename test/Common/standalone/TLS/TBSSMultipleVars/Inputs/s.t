SECTIONS {
  .foo    : { *(.text.foo) }
  .tbss.a : { *(.tbss.tls_a) }
  .tbss.b : { *(.tbss.tls_b) }
  .tbss.c : { *(.tbss.tls_c) }
  .data   : { *(.data.data) }
}
