SECTIONS {
  /DISCARD/ : { *(.ARM.exidx*) *(.gnu.linkonce.armexidx.*) }
  . = 0x10000;
  .text : { *(.text*) }
}
