SECTIONS {
  /* text section will be 0x16 bytes after relaxations are handled. Set address
     so it will spill over 0x1000 boundary before relaxation, but not after. */
  text (0xFE9) : {
    *(.text)
  }
  /* Before relaxation, this should be treated as if it were at 0x2000. After,
     it should be at 0x1000. */
  datas : ALIGN(4K) {
    *(.data)
    __global_pointer$ = .;
    *(.sdata)
  }
  /* Set to 0x2100 so that it will be in range of gp before relaxation, but not
     after while also not overlapping with 'datas'. */
  a_sec (0x2100) : {
    *(.a_sec)
  }
}