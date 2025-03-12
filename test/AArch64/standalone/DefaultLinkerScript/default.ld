PHDRS {
  CODE PT_LOAD;
  DATA PT_LOAD;
}

SECTIONS {
  DEFAULT_CODE : {
    *(.text*)
  } : CODE

  DATA : {
    foobar = 0x123;
  } : DATA
}
