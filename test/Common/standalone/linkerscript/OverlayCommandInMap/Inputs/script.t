MEMORY {
  ROM (rx) : ORIGIN = 0x1000, LENGTH = 0x10000
  RAM (rw) : ORIGIN = 0x2000, LENGTH = 0x10000
}

PHDRS {
  PHDR1 PT_LOAD;
}

SECTIONS {
  OVERLAY 0x1000 : NOCROSSREFS AT(0x2000) {
    .ov1 { *(.text.ov1) }
    .ov2 { *(.text.ov2) }
  } >ROM AT>RAM :PHDR1

  .text : { *(.text*) } >ROM :PHDR1
}
