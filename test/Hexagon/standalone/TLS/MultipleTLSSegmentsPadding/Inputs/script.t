PHDRS {
  text PT_LOAD;
  data PT_LOAD;
  tls_init PT_TLS;
  tls PT_TLS;
}
SECTIONS {
  .text 0x100000 : { *(.text*) } :text
  .data 0x200000 : { *(.data*) } :data
  .tdata         : { *(.tdata*) } :data :tls_init
  .tbss (NOLOAD) : { *(.tbss*) } :tls
}
