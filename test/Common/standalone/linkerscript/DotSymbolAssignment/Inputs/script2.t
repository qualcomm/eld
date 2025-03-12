SECTIONS {
  .text 0x2000 : {
    . = 0x10;
    *(.text)
  }
  .text.foo : { *(.text.foo) }
  /DISCARD/ : { *(.ARM.exidx.text.foo) }
}