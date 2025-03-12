PHDRS {
  CODE PT_LOAD;
  DATA PT_LOAD;
}
SECTIONS {
CODE1 0x0: {
         *(.text.caller*)
       } : CODE

CODE2 0x800000: {
         *(.text .text.*)
       } : CODE

DATA : {
         *(.data .data.* .bss)
       } : DATA
}
