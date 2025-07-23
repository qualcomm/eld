MEMORY {
  VMA_REGION (rwx) : ORIGIN = 0x1000, LENGTH = 0x1000
  LMA_REGION (rwx) : ORIGIN = 0x3fc0, LENGTH = 0x1000
}

PHDRS {
  A PT_LOAD;
  B PT_LOAD;
}

SECTIONS {
  .a : ALIGN_WITH_INPUT {
    begin = .;
    *(.a)
  } > VMA_REGION AT > LMA_REGION :A
  .b :  {
    *(.b)
    end = .;
  } > VMA_REGION AT > LMA_REGION :B
}
