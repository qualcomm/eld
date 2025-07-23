MEMORY {
  VMA_REGION (rwx) : ORIGIN = 0x1000, LENGTH = 0x1000
  LMA_REGION (rwx) : ORIGIN = 0x3fc0, LENGTH = 0x1000
}

PHDRS {
  P PT_PHDR FILEHDR PHDRS;
  A PT_LOAD FILEHDR PHDRS;
  B PT_LOAD;
}

SECTIONS {
  . = . + SIZEOF_HEADERS;
  .a : ALIGN_WITH_INPUT {
    begin = .;
    *(.a)
  } > VMA_REGION AT > LMA_REGION :A
  .b :  {
    *(.b)
    end = .;
  } > VMA_REGION AT > LMA_REGION :B
  /DISCARD/ : { *(.eh_frame) *(.ARM.exidx*) *(.hexagon.attributes) *(.riscv.attributes) }
}
