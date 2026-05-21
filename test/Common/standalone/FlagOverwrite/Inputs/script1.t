SECTIONS {
  . = 0x80000;
  .text : {
    *(.text*)
    . = ALIGN(64);
    . = 0x80000+0x10-0x64;
  } =0x3020
  .comment : { *(.comment) }
  /DISCARD/ : { *(.ARM.exidx*) }
}