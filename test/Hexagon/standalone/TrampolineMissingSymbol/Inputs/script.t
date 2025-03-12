SECTIONS {
  .moo : { *(.text.moo) }
  .data : { *(.data) }
  . = 0xF0000000;
  .txt.foo: { *(.text.foo) *(.text.coo) *(.text.moo) }
}
