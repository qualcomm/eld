SECTIONS {
 . = 0x10000;
 .crap : {
  . = . + 0x04;
  *(.text.bar)
  . = . + 0x08;
  *(.text.foo)
  . = . + 0x16;
  *(.text.*)
  *(.text)
 }
}