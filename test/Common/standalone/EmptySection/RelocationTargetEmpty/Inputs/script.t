SECTIONS {
 . = 0x10000;
 .foo : {
  *(.text.*)
  *(.text)
 }
}
