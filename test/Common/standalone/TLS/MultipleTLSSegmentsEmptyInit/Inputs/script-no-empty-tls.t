PHDRS {
  text PT_LOAD;
  data PT_LOAD;
  tls PT_TLS;
}
SECTIONS {
  .text 0x100000 : { *(.text*) } :text
  .data 0x200000 : { *(.data*) } :data
  .tbss (NOLOAD) : { *(.tbss*) } :tls
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.eh_frame*) }
}
