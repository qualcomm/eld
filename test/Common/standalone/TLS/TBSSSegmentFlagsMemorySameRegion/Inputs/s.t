MEMORY {
  ram (rwx) : ORIGIN = 0x1000, LENGTH = 1M
}
SECTIONS {
  .foo  : { *(.text.foo) }  > ram
  .tbss : { *(.tbss.tbss) } > ram
  .data : { *(.data.data) } > ram
}
