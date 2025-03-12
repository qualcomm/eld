PHDRS{
  ROM_CODE PT_LOAD;
  RAM_DATA PT_LOAD;
}

SECTIONS {
  BSS1 : { *(.bss.A) } : RAM_DATA
  DAT1 : { *(data1) }
  BSS2 : { *(.bss.B) }
  DAT2 : { *(data2) }

  CODE : { *(.text) }: ROM_CODE
  /DISCARD/ : {*(.ARM.*)}
}
