SECTIONS {
  .foo : {
    __foo_start = .;
    *(.text.foo)
    __foo_end = .;
  }
}
