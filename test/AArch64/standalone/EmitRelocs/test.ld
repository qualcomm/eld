PHDRS {
  CODE PT_LOAD;
  DATA PT_LOAD;
}

SECTIONS {
  CODE : {
    *(.text mycode*)
  } : CODE

  DATA : {
    *(.data .data.* .bss mydata)
  } : DATA
}
