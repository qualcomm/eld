PHDRS {
  text PT_LOAD;
  data PT_LOAD;
  tls  PT_TLS;
}

SECTIONS {
  .text   : { *(.text*) *(.init*) } :text
  .rodata : { *(.rodata*) }         :text
  .data   : { *(.data*) }           :data
  .tdata  : { *(.tdata*) }          :data :tls
  .tbss  (NOLOAD) : { *(.tbss*) }   :tls
  .bss   (NOLOAD) : { *(.bss*) *(COMMON) } :data
}
