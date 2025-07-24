PHDRS {
  A PT_LOAD;
}

SECTIONS {
  u = v;
  FOO (0x1000): {
    start = .;
    *(.text.foo)
    end = .;
  } :A
  v = end - start;
}