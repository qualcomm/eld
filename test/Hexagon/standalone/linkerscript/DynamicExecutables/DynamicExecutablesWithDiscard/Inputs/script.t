PHDRS {
  CODE_RO PT_LOAD;
  DATA PT_LOAD;
  GOT PT_LOAD;
  DYN PT_LOAD;
  DYNAMIC PT_DYNAMIC;
  RELADYN PT_LOAD;
}
SECTIONS {
  .text : { *(.text) } :CODE_RO
  .data : { *(.data) } :DATA
  .dynamic : { *(.dynamic) }: DYN : DYNAMIC
  .rela.dyn : {*(.rel.dyn)} : RELADYN
  /DISCARD/ : { *(.hash) *(.dynsym) *(.dynstr) }
}
