MEMORY {
  DATA : ORIGIN = 0x88, LENGTH = 0x100
  RAM (rwx) : ORIGIN = 0x80002a50,  LENGTH = ((262144) << 10)
}

SECTIONS {
  data : { *(.data) *(.sdata) } > DATA
  text : { *(.text) *(.text.*) } > RAM
  tbss : { *(.tbss) } > RAM
  PROVIDE(__tbss_align = ALIGNOF(tbss));
}
