PHDRS {
  CODE PT_LOAD;
  DATA PT_LOAD;
  T1 PT_TLS;
  T2 PT_TLS;
}

SECTIONS {
  .text 0x1000 : { *(.text*) } :CODE
  .low  0x4000 : { *(.tdata.low*) }  :DATA :T2
  .high 0x5000 : { *(.tdata.high*) } :DATA :T1
}
