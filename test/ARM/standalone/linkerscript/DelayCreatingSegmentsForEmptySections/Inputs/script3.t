SECTIONS
{
  .text           : { *(.text*) } 
  .bss            : { *(.bss*) } 
  .sdata          : { *(.sdata) } 
  .sbss           : { *(.sbss) } 
  . = ALIGN (64);
  . = ALIGN (4096);
  QSR_STRING :
  {
   *(QSR_STR.fmt.rodata.*)
  } 
}
