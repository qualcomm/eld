SECTIONS {
  .plt : { *(.plt) }
  .got : { *(.got.plt) }
  . = 0xF0000000;
  .text : { *(.text*) }
  .tdata : { *(.tdata*) }
  .tbss : { *(.tbss*) }
}
