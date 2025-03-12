PHDRS {
  P PT_LOAD;
  A PT_LOAD;
  B PT_LOAD;
}

PLUGIN_ITER_SECTIONS("findusessymbols", "FINDUSES")

SECTIONS {
.foo : {
  *(.text.foo)
  *(.text.baz)
}:P
.bar : {
  *(.text.bar)
}:A
.plugin : {
  *(.pluginfoo)
}:B
}
