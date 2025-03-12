PLUGIN_OUTPUT_SECTION_ITER("outputchunkiteratormovebetweensections","GETOUTPUT");
SECTIONS {
.foo : {
  *(.text.foo)
  *(.text.baz)
  *(.text.bar)
  *(COMMON)
}
.bar : {
}
.plugin : {
  *(.pluginfoo)
}
.bss : {
  *(.scommon*)
}
}
