SECTIONS {
  .allfuncs : {
    *(.text.foo)
    *(.text.bar)
    *(.text.baz)
  }
}