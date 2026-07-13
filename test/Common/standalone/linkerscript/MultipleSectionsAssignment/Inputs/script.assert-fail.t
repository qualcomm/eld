SECTIONS {
  .text : {
    *(.text*)
  }
  a = u;
}
u = 0x3;
ASSERT(u == 0x3, "u value is 0x3");
SECTIONS {
  .data : {
    *(.data*)
  }
  b = u;
  ASSERT(b == 0x3, "b final value is 0x3");
}
u = 0x7;
