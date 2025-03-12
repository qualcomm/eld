SECTIONS {
  text : {
    PROVIDE(bar = 3);
    *(*foo*)
    PROVIDE(bar = 5);
    *(*baz*)
    PROVIDE(bar = 7);
    *(*main*)
    PROVIDE(bar = UNDEF_SYM);
  }
}