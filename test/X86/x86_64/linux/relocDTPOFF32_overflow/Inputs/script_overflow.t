SECTIONS {
  .text 0x100 : { *(.text*) }
  .tdata : {
    . = . + 0xd0000000;  /* +3.25GB */
    *(.tdata*)
  }
  .tbss : { *(.tbss*) }
}