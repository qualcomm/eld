v = 0xc;
ASSERT(v == 0xa, "v final value is 0xa");
SECTIONS {
  u = v;
  ASSERT(u == 0xc, "u final value is 0xc");
  .text : {
    *(.text*)
  }
  v = 0xa;
}
ASSERT(v == 0xa, "v final value is 0xa");
