PRINT("O: %o\n", 42);

SECTIONS {
  .text : { *(.text*) }
}

