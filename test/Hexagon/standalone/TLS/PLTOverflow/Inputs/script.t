SECTIONS {
  . = 0x1000;
  .plt : { *(.plt*) }
  . = 0xf0000000;
  .text : { *(.text.foo) }
}
