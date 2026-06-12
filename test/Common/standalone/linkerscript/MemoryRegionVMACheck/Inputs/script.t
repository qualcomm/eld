MEMORY {
  RAM : ORIGIN = 0x1000, LENGTH = 0x1000
  ROM : ORIGIN = 0x5000, LENGTH = 0x1000
}
SECTIONS {
  .foo (0x2500) : { *(.text.foo) } >RAM
  .bar : { *(.text.bar) } >RAM
}
