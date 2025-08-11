PHDRS {
  A PT_LOAD;
  B PT_LOAD;
}

SECTIONS {
  FOO (0x1000): {
    fn = u;
    u = bar;
    *(.text.foo)
  } :A

  BAR : {
    u = baz;
    *(.text.bar)
  } :B

  BAZ : {
    u = main;
    *(.text.baz)
  } :B

  u = foo;
}