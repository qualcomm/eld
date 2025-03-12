.section .eh_frame, "a"
  .long 12
  .long 0x00
  .byte 0x01

  .byte 0x52
  .byte 0x00


  .byte 0x01
  .byte 0x01


  .byte 0x01

  .byte 0xFE

  .byte 0xFF

  .long 12
  .long 0x14
  .word .eh_frame
