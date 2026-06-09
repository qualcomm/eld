PHDRS {
  text PT_LOAD;
  data PT_LOAD;
  tls  PT_TLS;
}
SECTIONS {
  .foo  : { *(.text.foo) }  :text
  .tbss : { *(.tbss.tbss) } :data :tls
  .data : { *(.data.data) } :data
}
