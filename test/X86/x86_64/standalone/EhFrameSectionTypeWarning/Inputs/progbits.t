SECTIONS {
  .text : { *(.text*) }
  .eh_frame (PROGBITS) : { *(.eh_frame) }
}
