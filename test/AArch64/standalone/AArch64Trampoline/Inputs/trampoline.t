SECTIONS {
  foo = 0x8100000;
  .text.main : { KEEP(*(.text.main)) }
}
