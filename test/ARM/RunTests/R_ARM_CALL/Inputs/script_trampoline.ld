ENTRY(_start)

SECTIONS
{
  . = 0x60010000;

  .text : { *(.text) }

  . = ALIGN(8);
  . = . + 0x1000;  /* 4KB stack */
  stack_top = .;

  /* Place distant callee in a far section for trampoline testing */
  . = 0x62020000;
  .text.distant : { *(.text.distant) }
}
