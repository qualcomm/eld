PHDRS {
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
  tls  PT_TLS  FLAGS(4);
}

SECTIONS {
  .text 0x100000 : { *(.text*) *(.init*) } :text
  .rodata        : { *(.rodata*) }          :text
  .data 0x500000 : { *(.data*) }            :data
  .tdata         : { *(.tdata*) }           :data :tls
  .tbss  (NOLOAD): { *(.tbss*) }            :tls
  .bss   (NOLOAD): { *(.bss*) *(COMMON) }   :data
}
