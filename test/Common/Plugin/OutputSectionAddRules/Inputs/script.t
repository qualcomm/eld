PLUGIN_OUTPUT_SECTION_ITER("outputsectionaddrules","ADDRULES");
SECTIONS {
.foo : {
  . = .;
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
