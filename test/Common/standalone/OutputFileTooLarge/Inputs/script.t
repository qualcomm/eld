MEMORY {
  TEXT_REGION : ORIGIN = 0x5000, LENGTH = 0xffffffff
  DATA_REGION : ORIGIN = 0xffff7fff, LENGTH = 0x1000
}

PHDRS {
  MY_HDR PT_LOAD;
}

SECTIONS {
  text : { *(.text) } > TEXT_REGION : MY_HDR
  data : { *(.data) } > DATA_REGION : MY_HDR

  /* just place these since clang creates them */
  .comment 0 : { *(.comment) }
  .shstrtab 0 : { *(.shstrtab) }
  .symtab 0 : { *(.symtab) }
  .strtab 0 : { *(.strtab) }
  .ARM.exidx 0 : { *(*exidx*) } > TEXT_REGION
  /* create a dummy section and make it large enough to overflow offset. we
   * should create the section headers at this overflowed offset, which
   * seems to result in corrupted headers.
   */
   .mydebug (PROGBITS) : { . = . + 0x100000; } > TEXT_REGION
}
