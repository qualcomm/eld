SECTIONS {
  . = u;
  .text : {
    *(.text*)
  }
  .exidx : { *(*exidx*) }
  u = 0x3000;
  DATA_START = DEFINED(DATA_START) ? DATA_START : .;
  . = ALIGN(DATA_START, 0x1000);
  .data : {
    *(.data)
  }
}

