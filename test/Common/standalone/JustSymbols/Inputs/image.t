SECTIONS {
  . = 0x1000;
  .sdata : { *(.sdata.*) }
  . = 0x200000;
  .text : { *(.text*) }
  /DISCARD/ : { *(.ARM.exidx*) }
}
