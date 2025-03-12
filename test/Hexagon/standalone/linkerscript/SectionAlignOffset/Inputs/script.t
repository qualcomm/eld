PHDRS {
 TXT PT_LOAD;
 DATA PT_LOAD;
}

SECTIONS {
  .text : {
    *(.text.foo)
  } :TXT
  .data1 : {
    *(.data.data1)
  } :DATA
  .data2 : {
    *(.data.data2);
  } :DATA
}
