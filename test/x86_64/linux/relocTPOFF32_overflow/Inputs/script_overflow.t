SECTIONS {
  .text 0x100 : { *(.text*) }
  .tdata : {
    *(.tdata*)
    . = . + 0xd0000000;  /* Add 3.25GB - causes TPOFF32 overflow */
  }
  .tbss : { *(.tbss*) }
}
