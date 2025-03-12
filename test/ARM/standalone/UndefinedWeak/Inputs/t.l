SECTIONS
{
  . = 0x8000000;
  .text : { *(.text) }
  .data : { *(.data) }
  .bss : { *(.bss) }
}
