SECTIONS {
  . = 0x1000;
  .sdata : { *(.sdata.*) }
  . = 0x0008000;
  .text : { *(.text*) }
  /DISCARD/ : { *(.ARM.exidx*) }
}
