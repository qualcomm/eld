SECTIONS {
  . = 0x0;
  .text.calls   : { *(.text.calls)   }
  .text.kobject : { *(.text.kobject) }
  . = 0x100021;
  .exit.text    : { *(.exit.text)    }
}
