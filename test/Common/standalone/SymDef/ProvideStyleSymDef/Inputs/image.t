SECTIONS {
  . = 0x1000;
  .sdata : { *(.sdata.*) }
  . = 0x20000;
  .text : { *(.text*) }
}
