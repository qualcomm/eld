MEMORY {
  RAM : ORIGIN = 0x1000, LENGTH = 0x1000
}

PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .empty (PROGBITS) : { . = 0x1200; } >RAM :A
  .text 0x1200 : { *(.text*) } >RAM :A
  .empty1 : { . = 0x1400; } >RAM :A
  /DISCARD/ : { *(.ARM*) *(.*.attributes) }
}
