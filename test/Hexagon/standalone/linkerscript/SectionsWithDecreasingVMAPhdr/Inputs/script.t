PHDRS {
  A PT_LOAD;
}

SECTIONS {
  a (0x8000) : { *(.text.foo) } : A
  b (0x7000) : { *(.text.bar) }
  c : { *(.text.car);  }
}
