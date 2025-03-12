PHDRS {
  P PT_LOAD;
}

PLUGIN_ITER_SECTIONS("matchsectionsandgetrawdata", "MATCHSECTIONSANDGETRAWDATA")

SECTIONS {
.strings : {
  *(.mystrings*)
}:P
}
