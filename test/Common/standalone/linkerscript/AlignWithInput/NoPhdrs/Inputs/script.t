MEMORY {
  VMA_REGION (rwx) : ORIGIN = 0x1000, LENGTH = 0x1000
  LMA_REGION (rwx) : ORIGIN = 0x3fc0, LENGTH = 0x1000
}

SECTIONS {
  .a : ALIGN_WITH_INPUT {
    begin = .;
    *(.a)
  } > VMA_REGION AT > LMA_REGION
  .b : ALIGN_WITH_INPUT {
    *(.b)
    end = .;
  } > VMA_REGION AT > LMA_REGION
}
