PHDRS
{
  MYPTPHDR PT_PHDR FILEHDR PHDRS;
  text PT_LOAD FILEHDR PHDRS;
  NON PT_NULL;
  EMPTY PT_NULL;
  data PT_LOAD;
  dynamic PT_DYNAMIC;
  tls PT_TLS;
  note PT_NOTE;
}


SECTIONS
{
  . = . + SIZEOF_HEADERS;
  .foo : { *(.text.foo) } :text
  .bar (NOLOAD) : { *(.text.bar) } :NON
  /DISCARD/ : { *(.eh_frame*) *(.ARM.attributes) *(.riscv.attributes) *(.ARM.exidx*) }
}
