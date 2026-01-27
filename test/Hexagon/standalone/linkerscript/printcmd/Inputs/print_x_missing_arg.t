PRINT("X: %x %X\n", 42);

SECTIONS {
  .text : { *(.text*) }
}

