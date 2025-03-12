SECTIONS
{
  .text           :
  {
    *(.text.foo)
    . = ALIGN(4);
  } =0x00c0007f
  . = 0x08000000;
  .text.far :
  {
    *(.text.far)
  }
}
