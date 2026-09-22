EXTERN(foo)
SECTIONS {
  marker = .;
  .text : {
    BYTE(0x11)
    *(.text*)
  }
}
