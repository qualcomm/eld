SECTIONS {
  . = 0x1000;
  .sdata : { *(.sdata.*) }
  . = 0xf0000000;
  .text : { *(.text*) }
  /DISCARD/ : { *(.ARM.exidx*) }
}
