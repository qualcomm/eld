PHDRS {
  A PT_LOAD;
  B PT_LOAD;
  C PT_TLS;
}
MEMORY {
  mem : ORIGIN = 0x1000, LENGTH = 0x1000
}

SECTIONS {
  .text : {
    *(.text*)
    . += 4;
  } > mem :A
  .tdata : { *(.tdata*) } > mem :B :C
  .tbss : { *(.tbss*) } > mem :B :C
}
