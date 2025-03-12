PHDRS {
  PHDR PT_PHDR;
  TEXT PT_LOAD;
  RODATA PT_LOAD;
  DATA PT_LOAD;
  DYNAMIC PT_LOAD;
  DUMMY PT_DYNAMIC;
}

SECTIONS {
  .rodata : {
              . = . + 100;
              *(.rodata*)
            } :RODATA
  .text : { *(.text*) } :TEXT
  .data : { *(.data*) } :DATA
}
