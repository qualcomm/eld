SECTIONS {
  FOO (0x1000) : {
    fn = u;
    u = v1;
    *(.text.foo)
  }
  v1 = v2;
  v2 = v3;
  v3 = v4;
  v4 = v5;
  v5 = v6;
  v6 = foo;
}