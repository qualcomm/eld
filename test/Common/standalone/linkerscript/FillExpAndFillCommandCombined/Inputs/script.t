SECTIONS {
  text (0x16) : {
    . = . + 4;
    FILL(0xabababab)
    . = . + 4;
    FILL(0xbcbcbcbc)
    . = . + 4;
    *(.text*)
    *(.ARM.exidx)
  } =0xdeadbeef
}
