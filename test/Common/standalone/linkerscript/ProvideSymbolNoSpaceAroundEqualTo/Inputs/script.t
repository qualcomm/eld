SECTIONS {
  . = 0x11;
  PROVIDE(foo=.);
  OUT: {
    PROVIDE(bar=foo);
  }
}
PROVIDE(baz=bar);