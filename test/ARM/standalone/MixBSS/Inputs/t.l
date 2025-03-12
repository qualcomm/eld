PHDRS {
  a PT_LOAD;
}

SECTIONS { 
  STACK : {
    . = . + 0x200;
  } : a

  .text : { *(.text) } 

  STACK2 : {
    . = . + 0x200;
  }
}

