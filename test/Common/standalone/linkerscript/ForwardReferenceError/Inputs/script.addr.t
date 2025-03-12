SECTIONS
{
  USER_CODE_SECTION ALIGN(ADDR(BAR),64):
  {
    *(.USER_CODE_SECTION);
  }
  BAR : { *(*bar*) }
  /DISCARD/ : { *(.ARM.exidx*) }
}
