SECTIONS {
 . = 0x10000;
 .foo : {
    KEEP(*(.text.baz))
  *(.text.*)
  *(.text)
 }
}