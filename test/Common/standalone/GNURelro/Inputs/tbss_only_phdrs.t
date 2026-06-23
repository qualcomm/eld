PHDRS {
  ph_text PT_LOAD;
  ph_tls  PT_TLS;
  ph_data PT_LOAD;
}
SECTIONS {
  .text  : { *(.text*) *(.eh_frame*) } :ph_text
  .tbss  : { *(.tbss*) }               :ph_tls :ph_data
  .data  : { *(.data*) }               :ph_data
}
