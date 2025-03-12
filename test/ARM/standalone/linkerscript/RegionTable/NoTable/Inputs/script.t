SECTIONS {
  .BSSA : { *(.bss.bssA) }
  .BSSB : { *(.bss.bssB) }
  .BSSC : { *(.bss.bssC) }
  .FOO : { *(.data.foo) }
}
