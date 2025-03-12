SECTIONS
{
  foo ((0x12340000 + 0x00010000) + 0x00001000) :
  {
    *(.text)
  }

  bar 0x12340000 :
  {
    *(.symtab)
  }
}
