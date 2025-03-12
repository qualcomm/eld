SECTIONS
{
  .text           :
  {
    *(.text.main)
    *(.text.callfar1)
    . = ALIGN(4);
    *(.text.callfar2)
    . = ALIGN(4);
  } =0x00c0007f
  . = 0x08000000;
  .text.far :
  {
    *(.text.far)
  }
}
