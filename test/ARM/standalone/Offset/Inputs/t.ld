PHDRS {
  RW PT_LOAD;
  CODE PT_LOAD;
}
SECTIONS {
  RW 0x1000: {
    *(.text .rodata .rodata.*)
  } : RW

  CO 0x148f5000: {
    KEEP(*(.foo))
  } : CODE

  RO ALIGN(16384): {
    * (.data*)
    * (.bss *.bss .common)
  }
}
