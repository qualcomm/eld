PHDRS {
  P PT_NULL;
  A PT_LOAD;
  B PT_LOAD;
}

SECTIONS {
.foo PLUGIN_CONTROL_MEMSZ("copy","COPYBLOCKS", "G0") : {
  *(.text.foo)
  . = . +100;
  *(.text.baz)
}:P
.bar : {
  *(.text.bar)
}:A
.plugin : {
  *(.pluginfoo)
}:B
}
