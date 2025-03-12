PHDRS {
 TXT PT_LOAD;
 DATA PT_LOAD;
}

SECTIONS {
  .text : {
    *(.text.foo)
    . = . + 0x1000;
  } :TXT
  . = ALIGN(8192);
  .data1 : {
    *(.data.data1)
  } :DATA
  .data2 : {
    *(.data.data2);
  } :DATA
}
