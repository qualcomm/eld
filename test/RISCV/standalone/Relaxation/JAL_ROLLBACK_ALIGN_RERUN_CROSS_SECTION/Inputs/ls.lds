SECTIONS {
  . = 0x0;
  .head      : { *(.head) }
  . = 0xFF808;
  .text      : { *(.text.calls) *(.text.kobject) }
  . = ALIGN(1 << 21);
  .exit.text : { *(.exit.text) }
}
