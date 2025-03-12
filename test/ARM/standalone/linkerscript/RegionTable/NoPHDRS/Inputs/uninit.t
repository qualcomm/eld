SECTIONS {
  .region_table : { *(__region_table__) }
  .BSSA : { *(.bss.bssA) }
  .BSSB (UNINIT) : { *(.bss.bssB) }
  .BSSC : { *(.bss.bssC) }
  .FOO : { *(.data.foo) }
}
