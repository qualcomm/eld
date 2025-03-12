PLUGIN_OUTPUT_SECTION_ITER("GetAllSymbols","GETSYMBOLS");
SECTIONS
{
  .text           :
  {
    *(.text.bar*)
    . = ALIGN(4);
  } =0x00c0007f
  . = 0xF0000000;
  .text.far :
  {
    *(.text.far*)
  }
}