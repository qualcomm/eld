SECTIONS
{
  .text           :
  {
    *(.text.foo*)
    . = ALIGN(4);
  } =0x00c0007f
  . = 0xF0000000;
  .text.far :
  {
    *(.text.far*)
  }
}
