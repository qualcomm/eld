PHDRS {
  text PT_LOAD;
}
SECTIONS {
  u1 = 0x1001;
  foo u1 : { *(.text.foo) } :text
  u2 = u1 + SIZEOF(foo);
  bar u2 : { *(.text.bar) } :text
  u3 = u2 + SIZEOF(bar);
  baz u3 : { *(.text.baz) } :text
}
