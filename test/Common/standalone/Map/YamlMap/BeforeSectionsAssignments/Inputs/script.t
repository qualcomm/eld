u = 0x13;
INCLUDE included.t
SECTIONS {
  .text : {
    u = 0x15;
    *(.text*)
  }
}
u = 0x17;
