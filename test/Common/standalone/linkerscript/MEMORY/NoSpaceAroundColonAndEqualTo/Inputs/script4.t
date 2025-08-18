MEMORY {
  RAM : ORIGIN= 0x1000, LENGTH = 0x1000
}

SECTIONS {
  .foo : { *(.text.foo) } >RAM
  .text : { *(*text*) } >RAM
}
