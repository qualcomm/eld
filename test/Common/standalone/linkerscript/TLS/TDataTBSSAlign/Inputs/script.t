MEMORY {
  mem : ORIGIN = 0x1000, LENGTH = 0x1000
}

SECTIONS {
  .text : {
    *(.text*)
    . += 4;
  } > mem
  .tdata : { *(.tdata*) } > mem
  .tbss : { *(.tbss*) } > mem
}
