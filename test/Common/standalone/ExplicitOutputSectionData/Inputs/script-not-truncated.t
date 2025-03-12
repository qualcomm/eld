SECTIONS {
  .text : {
    QUAD(0xeeeeeeeeeeeeeeee)
    QUAD(-0xeeeeeeeeeeeeeeee)
    SQUAD(0xdddddddddddddddd)
    SQUAD(-0xdddddddddddddddd)
    LONG(0xcccccccc)
    LONG(-0x7ccccccc)
    SHORT(0xbbbb)
    SHORT(-0x7bbb)
    BYTE(-0x7a)
    BYTE(0xaa)
  }
}