SECTIONS
{
  .mybss : { *(.bss .bss.*) }
  .bss : { *(.bss) *(COMMON) }
}


