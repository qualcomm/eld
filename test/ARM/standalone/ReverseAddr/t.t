PHDRS {
  A PT_LOAD;
}

SECTIONS
{
  foo ((0x12340000 + 0x00010000) + 0x00001000) :
  {
    *(.text)
  } : A

  bar 0x12340000 :
  {
    *(.data)
  } : A
}
