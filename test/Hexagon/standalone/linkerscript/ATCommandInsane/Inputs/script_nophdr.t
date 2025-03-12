SECTIONS {
  .foo (0xf0004000) : AT(0x4000) { *(.text.foo) }
  . = ALIGN(256K);
  .boo : {}
  .bar : { *(.text.bar) }
  . = ALIGN(64K);
  .bak : {}
  .baz : { *(.text.baz) }
}
