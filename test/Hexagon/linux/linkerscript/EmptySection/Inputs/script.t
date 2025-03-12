SECTIONS {
  . = 0xd0000000;
  .candidate_compress_section :
  {
    *(.text.foo)
  }
  . = 0x01D00000;
  .BOOTUP : {}
  .bar : {
    *(.text.bar)
  }
}
