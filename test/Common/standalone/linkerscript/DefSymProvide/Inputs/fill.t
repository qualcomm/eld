SECTIONS {
  PROVIDE(fillval = 10);
  .foo : {
    *(.text.foo)
    FILL(fillval);
    *(.text.baz)
  }
  /DISCARD/ : { *(.ARM.exidx*) *(.*.attributes) }
}
