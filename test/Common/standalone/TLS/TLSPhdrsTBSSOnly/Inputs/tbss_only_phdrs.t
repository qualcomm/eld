PHDRS {
  text PT_LOAD;
  tls  PT_TLS;
  data PT_LOAD;
}

SECTIONS {
  .text  : { *(.text*) *(.eh_frame*) } :text
  .tbss  : { *(.tbss*) }               :tls :data
  .data  : { *(.data*) }               :data
}
