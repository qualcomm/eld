SECTIONS {
  u = v;
  FOO (0x1000): {
    start = .;
    *(.text.foo)
    end = .;
  }
  v = end - start;
}