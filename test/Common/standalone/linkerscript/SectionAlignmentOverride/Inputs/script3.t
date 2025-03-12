SECTIONS {
  .foo : { *(.text.foo) }
  .text (204) : { *(.text*) }
}
