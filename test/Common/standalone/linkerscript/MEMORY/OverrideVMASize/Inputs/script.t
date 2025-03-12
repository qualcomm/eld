MEMORY {
  RAM : ORIGIN = 0x1000, LENGTH = 0x1000
}

SECTIONS {
  .empty : { . = 0x1200; } > RAM
  .text 0x1200 : { *(.text*) } > RAM
  .empty1 : { . = 0x1400; } > RAM
  /DISCARD/ : { *(.ARM*) *(.*.attributes) }
}
