SECTIONS {
  foo = 0x8000000;
  .text.main : { KEEP(*(.text.main)) }
}
