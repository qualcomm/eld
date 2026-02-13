SECTIONS {
  .text.anchor : { *(.text.anchor) }
  OVERLAY 0x1000 : {
    .ovl1 { *(.text.ovl1) } INSERT AFTER .text.anchor
  }
}
