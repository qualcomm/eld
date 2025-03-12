SECTIONS {
 . = 0x10000;
 .crap : {
  *(.text.bar)
  *(.text.foo)
  *(.text.*)
  *(.text)
 }
 /DISCARD/ : { *(.ARM.exidx*) }
}