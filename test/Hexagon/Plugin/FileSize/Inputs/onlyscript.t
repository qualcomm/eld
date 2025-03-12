PHDRS {
  A PT_LOAD;
}

SECTIONS {
.foo : {
  *(.text.foo)
  . = ALIGN(32);
  *(.text.baz)
}:A
.bar : {
  *(.text.bar)
}:A
}
