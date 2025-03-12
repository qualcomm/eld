SECTIONS {
  .foo : { *(.text.foo) }
  .text ALIGN(0x4) :  { *(.text*) }
}
