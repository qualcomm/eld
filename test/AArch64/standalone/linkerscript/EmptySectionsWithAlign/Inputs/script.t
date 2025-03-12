SECTIONS {
  .foo : {
      *(.text.foo)
  }
  . = ALIGN(1M);
  . = ALIGN(4K);
  .dummy : {}
  .bar : {
    *(.text.bar)
  }
  .data : {
      *(.data)
  }
}
