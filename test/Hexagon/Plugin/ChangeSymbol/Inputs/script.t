PLUGIN_OUTPUT_SECTION_ITER("changesymbol","CHANGESYMBOL");
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
. = 40;
my_data_symbol = .;
.data : {
  my_data_symbol_inside = .;
  *(.data)
}
. = 0xF0000000;
.car : {
  *(.text.car)
}
}
