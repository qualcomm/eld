SECTIONS {
  .moo : { *(.text.moo) }
  .init : { *(.init) }
  .data : { *(.data) }
  . = 0xF0000000;
  .txt.foo : { *(.text.foo) *(.text.coo) *(.text.moo) }
}
