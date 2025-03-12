PHDRS {
INIT PT_LOAD;
MYTEXT PT_LOAD;
COMPRESS PT_LOAD;
};

SECTIONS {
  . = 0xD0000000;
  .compress_sect : {
    *(.text.compress)
  } :COMPRESS

  . = 0x1300000;
  .BOOTUP : {} :INIT

  .start : {
    *(.text.foo)
  } : INIT

  .sbss : {
     *(.sbss)
  }

  .dump (NOLOAD) : {
    *(.bss.*)
    *(.text.bar)
  }

  .mytext : {
   *(.text.mytext)
  } :MYTEXT

  .bss : {
    *(*.bss)
  }

  __unrecognized_start__ = . ;
  .unrecognized : { *(*) }
  __unrecognized_end__ = . ;
}
