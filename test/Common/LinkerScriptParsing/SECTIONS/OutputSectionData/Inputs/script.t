SECTIONS {
  .text : {
    *(.text*)
    LONG(2)
  }
  .one : { BYTE(1) }
  .some_value : { LONG(15) }
  .empty : { SHORT(0) }
  .values : { QUAD(0x0000000100000001); QUAD(0x0000000500000005) }
}