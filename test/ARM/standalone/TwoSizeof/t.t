SECTIONS
{
  S2 : {
    *(.data)
  }
  S1 : {
    *(.text)
    ASSERT(SIZEOF(S1) + SIZEOF(S2) < 0x20, "Invalid Size");
  }
}