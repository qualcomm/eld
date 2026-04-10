SECTIONS {
  .text : { *(.text*) }
  .rodata : {
    ASCIZ ("hello")
    ASCIZ ("world")
  }
}
