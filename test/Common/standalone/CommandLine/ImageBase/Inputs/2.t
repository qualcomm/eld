SECTIONS
{
  . = 0x90000000;
  .text : { *(.text) *(.text.*) }
}
