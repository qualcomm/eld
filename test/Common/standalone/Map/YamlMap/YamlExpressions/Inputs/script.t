SECTIONS {
  .text : {
    __start_section_foo = .;
    *(.text*)
    *(.text.baz)
    __end_section_foo = .;
  }
}
