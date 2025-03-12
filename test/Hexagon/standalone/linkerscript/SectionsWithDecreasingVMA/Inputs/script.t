SECTIONS {
  a (0x8000) : { *(.text.foo) }
  b (0x7000) : { }
  c : { *(.text.bar) }
}
