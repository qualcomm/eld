var_u = 0x1;
SECTIONS {
  .foo : ALIGN(0x40) {}
  var_v = (DEFINED(var_u) ? ALIGNOF(.foo) : 0x11);
}

