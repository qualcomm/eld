SECTIONS {
  . = 0x0;
  . += 0xFF808;
  .text : {
    *(.text.calls)
    *(.text.kobject)
  }
  . = ALIGN(1 << 21);
  .exit.text : { *(.exit.text) }
}
