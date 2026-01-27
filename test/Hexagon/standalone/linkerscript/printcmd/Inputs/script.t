PRINT("PRINTCMD pre a=%d b=0x%x", 4 + 5, 0x2a);

SECTIONS {
  .text : { *(.text*) }
  PRINT("PRINTCMD post sum=%d dot = 0x%x", (1 + 2) * 3, .);
  PRINT("PRINTCMD post dot=0x%x", .);
  .data : { *(.data*) }
}
