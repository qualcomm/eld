PHDRS {
  CODE PT_LOAD;
  DATA PT_LOAD;
  T1 PT_TLS;
  T2 PT_TLS;
}

SECTIONS {
  .text (0x1000) : { *(.text*) } :CODE
  .tdata (0x4000) : { *(.tdata*) } :DATA :T1
  .tbss : ALIGN(0x400) { *(.tbss*) } :DATA :T2
}