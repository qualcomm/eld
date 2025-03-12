PLUGIN_OUTPUT_SECTION_ITER("outputsectioniteratorconfig","GETOUTPUTCONFIG", "foo.txt");
SECTIONS {
.foo : {
  *(.text.foo)
  *(.text.baz)
  *(COMMON)
}
.bar : {
  *(.text.bar)
}
.data : {
  *(.data)
}
}
