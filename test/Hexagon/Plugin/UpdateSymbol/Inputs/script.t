PLUGIN_ITER_SECTIONS("updatesymbol","UPDATESYMBOL");
SECTIONS {
.foo : {
  *(.text.foo)
}
.bar : {
  *(.text.bar)
}

.baz : {
  *(.text.baz)
}
}
