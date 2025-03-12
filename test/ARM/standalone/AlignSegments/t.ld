PHDRS {
  CODE PT_LOAD;
  RW PT_LOAD;
}
SECTIONS {
  CODE : {
    KEEP(*("ENTRY"))
  } : CODE

  RO : {
    *(.text .rodata .rodata.*)
  }

  RW 0x10000: {
    * (.data)
    * (.bss *.bss .common)
  } : RW
}
