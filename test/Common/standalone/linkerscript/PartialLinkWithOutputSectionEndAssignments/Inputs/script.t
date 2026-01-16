u1 = 0x100;
SECTIONS {
  u2 = 0x300;
  foo : {
    *(.text.foo)
    u3 = 0x500;
  }
  u4 = 0x700;
  bar : {
    u5 = 0x900;
    *(.text.bar)
  }
  u5 = 0x1100;
}
u6 = 0x1300;
