/* foo is defined in 1.c */
PRINT("S: %s\n", foo);

SECTIONS {
  .text : { *(.text*) }
}

