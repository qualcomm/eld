PHDRS {
  P PT_LOAD;
  A PT_LOAD;
  B PT_LOAD;
}

SECTIONS {
.foo PLUGIN_CONTROL_FILESZ("copy","COPYBLOCKS", "G0") : {
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
