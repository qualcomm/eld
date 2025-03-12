SECTIONS {
  .foo : { *(.text.foo) }
  .text ALIGN(10) : { *(.text*) }
}
