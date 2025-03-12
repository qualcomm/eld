PLUGIN_OUTPUT_SECTION_ITER("outputchunkiteratorsymbols","GETOUTPUT");
SECTIONS {
.foo : {
  *(.text.foo)
  *(.text.baz)
  *(.text.bar)
  *(.scommon*)
  *(COMMON)
}
.plugin : {
  *(.pluginfoo)
}
}
