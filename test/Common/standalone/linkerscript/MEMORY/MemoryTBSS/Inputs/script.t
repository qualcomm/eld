MEMORY {
  MYMEM (rwx) : ORIGIN = 0x1000, LENGTH = 0x80000
}

SECTIONS {
  .foo : { *(.text.foo) } > MYMEM
  .tbss : { *(.tbss.a) } > MYMEM
  .mydata  : { *(.data.b) } > MYMEM
  .otherdata  : { *(.data*) } > MYMEM
  /DISCARD/ : { *(.ARM.exidx*) *(.eh_frame) }
}
