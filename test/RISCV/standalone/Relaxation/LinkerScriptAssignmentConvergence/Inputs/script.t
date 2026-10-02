PHDRS {
  A PT_LOAD;
}

SECTIONS {
  u = v;
  foo (0x1004) : {
    *(.text.foo*)
  } :A
  v = v1;
  v1 = .;

  bar (0x101000) : {
    *(.text.bar)
  } :A
}
