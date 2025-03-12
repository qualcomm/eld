SECTIONS {
  .text : {
    *(.text.foo)
   . = ALIGN(64);
    *(.text.bar)
  }
}