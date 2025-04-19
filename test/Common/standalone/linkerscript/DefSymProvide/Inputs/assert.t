SECTIONS {
  .text : { *(.text*) }
  PROVIDE(foo = 0);
  PROVIDE(__text_align = ALIGNOF(.text));
  ASSERT(__text_align > foo, "good!")
  /DISCARD/ : { *(.ARM.exidx*) *(.*.attributes) }
}
