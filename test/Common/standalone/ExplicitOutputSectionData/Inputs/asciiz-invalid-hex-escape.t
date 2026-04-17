SECTIONS {
  .text : { *(.text*) }
  .rodata : {
    ASCIZ "A\x41"
  }
}
