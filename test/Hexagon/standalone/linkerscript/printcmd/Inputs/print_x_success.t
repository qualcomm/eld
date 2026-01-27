PRINT("X: %x %X\n", 42, 42);

SECTIONS {
  .text : { *(.text*) }
}

