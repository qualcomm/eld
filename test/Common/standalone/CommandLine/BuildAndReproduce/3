__my_image_start = 0x80000;
SECTIONS {
  .foo (0x8000) : {
    foo.o*(.text*)
  }
  INCLUDE bar.t
  /DISCARD/ : { *(.ARM.exidx*) }
}
__my_image_end = 0x80000;
