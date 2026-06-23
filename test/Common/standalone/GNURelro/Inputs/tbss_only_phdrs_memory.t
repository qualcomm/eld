PHDRS {
  ph_text PT_LOAD;
  ph_tls  PT_TLS;
  ph_data PT_LOAD;
}
MEMORY {
  FLASH (rx)  : ORIGIN = 0x400000, LENGTH = 0x10000
  RAM   (rwx) : ORIGIN = 0x500000, LENGTH = 0x10000
}
SECTIONS {
  .text  : { *(.text*) *(.eh_frame*) } > FLASH :ph_text
  .tbss  : { *(.tbss*) }               > RAM   :ph_tls :ph_data
  .data  : { *(.data*) }               > RAM   :ph_data
}
