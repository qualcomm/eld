
MEMORY {
  FOO (rwx) : ORIGIN = 0x1000, LENGTH = 0x4000
}

PHDRS {
  A PT_LOAD;

}
SECTIONS {
  VMA = 0x500;
  .foo (PROGBITS) : {
    BYTE(1);
    . = ALIGN(0x100);
   } :A
  .bar VMA : {
     ASSERT(. != 0x1100, "value of . should be 0x500")
     *(.text.bar)
   }
   /DISCARD/ : { *(.ARM.exidx*) *(.eh_frame*) }
}
