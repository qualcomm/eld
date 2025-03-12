SECTIONS
{
  .ukernel.island : { *(*.ukernel.island) }
  .text           : { *(.text) }
}
