SECTIONS {
  .text : { *(.text*) }
  .rodata : {
    ASCIZ "A\nB\rC\tD\101\0E"
  }
}
