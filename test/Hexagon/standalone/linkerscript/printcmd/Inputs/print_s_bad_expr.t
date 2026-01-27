PRINT("S: %s\n", 1 + 2);

SECTIONS {
  .text : { *(.text*) }
}

