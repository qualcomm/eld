MEMORY {
  rom (rx) : ORIGIN = 0x1000, LENGTH = 1M
  ram (rw) : ORIGIN = 0x100000, LENGTH = 1M
}
SECTIONS {
  .foo  : { *(.text.foo) }  > rom
  .tbss : { *(.tbss.tbss) } > ram
  .data : { *(.data.data) } > ram
}
