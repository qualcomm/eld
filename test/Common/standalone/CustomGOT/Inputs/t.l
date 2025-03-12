SECTIONS {
  .TBASE_APP_CODE : {
    *(.text .text.*)
    *(.rodata .rodata.*)
  }
  .got : ALIGN(4096) {
    KEEP(*.t.o (.data))
  }
}
