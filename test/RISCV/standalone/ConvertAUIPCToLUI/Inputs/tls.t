MEMORY {
  RAM (rwx) : ORIGIN = 0x80002a50,  LENGTH = ((262144) << 10)
}

SECTIONS {
  text : { *(.text) } > RAM
  data : { *(.data); *(.sdata) } > RAM
  tbss : { *(.tbss) } > RAM
  PROVIDE(__tbss_align = ALIGNOF(tbss));
}
