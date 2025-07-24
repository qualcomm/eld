SECTIONS {
  FOO (0x1000): {
    fn = u;
    u = bar;
    *(.text.foo)
  }

  BAR : {
    u = baz;
    *(.text.bar)
  }

  BAZ : {
    u = main;
    *(.text.baz)
  }

  u = foo;
}