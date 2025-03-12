SECTIONS {
  text: {
    *(*text*)
    PROVIDED_SYM = 11;
    PROVIDE(PROVIDED_SYM = UNDEF_SYM_1);
    PROVIDE(foo = UNDEF_SYM_2);
  }
}