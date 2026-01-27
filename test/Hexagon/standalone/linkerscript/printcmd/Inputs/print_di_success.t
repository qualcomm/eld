PRINT("DI: d=%d i=%i\n", 42, -1);

SECTIONS {
  .text : { *(.text*) }
}

