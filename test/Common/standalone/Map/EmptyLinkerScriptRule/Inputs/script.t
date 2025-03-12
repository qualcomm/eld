SECTIONS {
  .foo : {
           *(.text.bar)
           *(.text.baz)
           *(.text.foo)
           *(.text.boo)
          }
}
