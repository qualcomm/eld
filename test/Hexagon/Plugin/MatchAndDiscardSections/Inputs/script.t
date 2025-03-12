PHDRS {
  P PT_LOAD;
  A PT_LOAD;
}

PLUGIN_SECTION_MATCHER("matchanddiscardsections", "MATCHANDDISCARDSECTIONS")

SECTIONS {
.island : {
  *(.text.island*)
}:P
.bar : {
  *(.text.bar)
}:A
}
