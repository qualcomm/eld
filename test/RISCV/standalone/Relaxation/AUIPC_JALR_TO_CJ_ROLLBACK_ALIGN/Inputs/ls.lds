SECTIONS {
  . = 0x0;
  .text : {
    *(.text.calls)
    *(.text.kobject)
  }
  . = ALIGN(16);
  .exit.text : { *(.exit.text) }
}
