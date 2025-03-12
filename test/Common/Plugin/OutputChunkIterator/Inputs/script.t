PLUGIN_OUTPUT_SECTION_ITER("outputchunkiterator","GETOUTPUT");
SECTIONS {
 . = 0xF00000000;
.foo : {
  *(.text.foo)
  *(.text.baz)
  *(COMMON)
}
.bar : {
  *(.text.bar)
}
.plugin : {
  *(.pluginfoo)
}
.bss : {
  *(.scommon*)
}
}
