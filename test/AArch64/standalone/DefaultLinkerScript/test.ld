PHDRS {
  CODE PT_LOAD;
  DATA PT_LOAD;
}

SECTIONS {
  CODE : {
    *(.text.foo*)
    *(.text.bar*)
  } : CODE

  DATA : {
    *(.data .data.* .bss mydata)
  } : DATA
}
