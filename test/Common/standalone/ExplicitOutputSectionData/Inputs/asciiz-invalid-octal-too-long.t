SECTIONS {
  .text : { *(.text*) }
  .rodata : {
    ASCIZ "A\1234"
  }
}
