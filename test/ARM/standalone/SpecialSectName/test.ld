PHDRS {
  CODE PT_LOAD;
  RW PT_LOAD;
}
SECTIONS {
  "ENTRY" : {
    KEEP(*("ENTRY"))
  } : CODE

  .ARM.exidx : {
    *(.ARM.exidx*)
  }

  RO : {
    *(.text .rodata .rodata.*)
  }

  RW 0x10000: {
    * (.data)
    * (.bss *.bss .common)
  } : RW
}
