SECTIONS {
  . = 0x1000 ^ 0x0100;
  . ^= 0x0010;
  .text : { *(.text.foo) }
}
