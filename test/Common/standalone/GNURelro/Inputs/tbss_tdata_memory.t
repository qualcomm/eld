MEMORY {
  FLASH (rx)  : ORIGIN = 0x400000, LENGTH = 0x10000
  RAM   (rwx) : ORIGIN = 0x500000, LENGTH = 0x10000
}
SECTIONS {
  .text  : { *(.text*) *(.eh_frame*) } > FLASH
  .tdata : { *(.tdata*) }              > RAM
  .tbss  : { *(.tbss*) }               > RAM
  .data  : { *(.data*) }               > RAM
}
