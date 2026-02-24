SECTIONS {
  OVERLAY 0x1000 : {
    .ov1 { *(.text.ov1) } >ROM
  }
}

