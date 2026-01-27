PRINT("DI: d=%d i=%i\n", 42);

SECTIONS {
  .text : { *(.text*) }
}

