ENTRY(_start)

SECTIONS
{
  . = 0x60010000;

  .text : { *(.text) }

  . = ALIGN(8);
  . = . + 0x1000;  /* 4KB stack */
  stack_top = .;
}