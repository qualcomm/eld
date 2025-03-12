PHDRS {
  P PT_LOAD;
  A PT_LOAD;
}

PLUGIN_ITER_SECTIONS("matchislandsectionsgetsymbols", "MATCHFINDUSESANDGETSYMBOLS")

SECTIONS {
.island : {
  *(.text.island*)
}:P
.bar : {
  *(.text.bar)
}:A
}
