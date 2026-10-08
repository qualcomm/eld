SECTIONS {
  . = 0;
  .text : { *(.text) }

  /* During LUI relaxation, pending C.JAL deletion makes B == 0x7ff, which
     fits zero-page (x0 +/- 2KB). After C.JAL is rolled back (the call no
     longer fits in range once the LUI is kept), B shifts to 0x805, which no
     longer fits -- the zero-page LUI deletion must be rolled back too. */
  B = . + 2031;

  G = 0x804;
  target = 0x806;
  __global_pointer$ = 0x800;
}
