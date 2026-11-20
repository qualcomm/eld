PHDRS {
  text PT_LOAD;
  data PT_LOAD;
  tls_init PT_TLS;
  tls PT_TLS;
}
SECTIONS {
  .text 0x100000 : { *(.text*) } :text
  .tdata 0x200000 : { *(.tdata*) } :data :tls_init
  .data 0x200000 : { *(.data*) } :data
  .tbss (NOLOAD) : { *(.tbss*) } :tls
}
