SECTIONS {
  .foo : { *(.text.foo) }
  .bar : {
    . = ALIGN(8);
    BYTE(0x8)
    . = . + 1000000;
  }
}