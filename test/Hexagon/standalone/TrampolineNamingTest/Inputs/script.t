SECTIONS {
  .main : { *(.text) }
  . = 0xF0000000;
  .fb : { *(.text.foobar) }
}