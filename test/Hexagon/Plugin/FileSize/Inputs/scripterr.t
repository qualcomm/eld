PHDRS {
  A PT_LOAD;
}

SECTIONS {
.foo PLUGIN_CONTROL_MEMSZ("filesize","COPYBLOCKS", "G0") : {
  *(.text.foo)
  . = ALIGN(32);
  *(.text.baz)
}:A
.bar : {
  *(.text.bar)
}:A
}
