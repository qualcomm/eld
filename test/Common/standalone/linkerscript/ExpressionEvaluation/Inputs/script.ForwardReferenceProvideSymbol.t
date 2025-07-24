SECTIONS {
  u = v ? v + 0x10 : 0x20;
  FOO (0x1000): {
    start = .;
    *(.text.foo)
    end = .;
  }
  PROVIDE(v = end - start);
}