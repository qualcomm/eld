MEMORY {
  mymem : ORIGIN = 0x100, LENGTH = 0x200
}

REGION_ALIAS("memory", "mymem")

SECTIONS {
   myorigin = ORIGIN(myymem);
   mylen = LENGTH(mymem);
   mylen = LENGTH(memory);
  .text : { *(.text*) } > mymem
  /DISCARD/ : { *(.ARM.exidx*)  }
}
