SECTIONS {
  .text : {
    LONG(0x1cccccccc)
    LONG(-0xcccccccc)
    LONG(-0x1cccccccc)
    SHORT(0x1bbbb)
    SHORT(-0xbbbb)
    SHORT(-0x1bbbb)
    BYTE(0x1aa)
    BYTE(-0xaa)
    BYTE(-0x1aa)
  }
}