PHDRS {
  SEGA PT_LOAD;
}

SECTIONS {
  .region_table : { *(__region_table__) } :SEGA
  .BSSA : { *(.bss.bssA) } :SEGA
  .BSSB : { *(.bss.bssB) } :SEGA
  .BSSC : { *(.bss.bssC) } :SEGA
  .FOO : { *(.data.foo) } :SEGA
}
