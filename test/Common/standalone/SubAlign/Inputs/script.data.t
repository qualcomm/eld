SECTIONS {
  .explicit_data : SUBALIGN(64) {
    BYTE(0x11)
    SHORT(0x2222)
    LONG(0x44444444)
    QUAD(0x8888888888888888)
    *(.text*)
    BYTE(0xAA)
    QUAD(0xBBBBBBBBBBBBBBBB)
  }
  /DISCARD/ : { *(.ARM.exidx*) }
}
