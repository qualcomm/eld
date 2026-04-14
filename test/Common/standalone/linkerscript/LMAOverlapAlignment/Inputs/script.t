SECTIONS {
  u1 = 0x1001;
  foo u1 : {
    *(.text.foo)
  }
  u2 = u1 + SIZEOF(foo);
  bar u2 : {
    *(.text.bar)
  }
  u3 = u2 + SIZEOF(bar);
  baz u3 : {
    *(.text.baz)
  }
}
