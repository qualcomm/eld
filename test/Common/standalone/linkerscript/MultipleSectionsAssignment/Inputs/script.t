SECTIONS {
  .text : {
    *(.text*)
  }
  a = u;
}
u = 0x3;
SECTIONS {
  .data : {
    *(.data*)
  }
  b = u;
}
u = 0x7;
