SECTIONS
{
  INCLUDE include_script.t
  USER_CODE_SECTION 0x4200000 :
  AT (0x4200000)
  {
    *(.text)
  }
  USER_BSS_SECTION 0x4201000 : AT(0x4201000) { LONG(0X0); }
  /DISCARD/ : { *(.ARM.exidx*) }
}
