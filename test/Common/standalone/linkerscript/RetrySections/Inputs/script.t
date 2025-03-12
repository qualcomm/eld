SECTIONS {
  .text.foo : {
  }

  .text.bar : {
    *(.text.foo)
    *(.text.bar)
  }

  .text.baz : {
    *(.text.foo)
  }
}
