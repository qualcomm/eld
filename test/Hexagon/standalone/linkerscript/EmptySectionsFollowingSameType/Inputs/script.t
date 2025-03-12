SECTIONS {
  .text.foo : { *(.text.foo) }
  .empty : {}
  .text.bar : { *(.text.bar) }
}
