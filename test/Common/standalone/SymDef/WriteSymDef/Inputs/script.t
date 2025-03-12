SECTIONS {
  .foo (0x100) : { 
    *(.text.foo)
  }
  .text : {
    *(.text.*)
  }
}
