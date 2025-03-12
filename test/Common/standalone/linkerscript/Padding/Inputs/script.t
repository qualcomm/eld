SECTIONS {
 .text : {
    *(.text.fn)
    . = . + 1000;
    FILL(0x1234);
    *(.text*)
  } =0x100
}
