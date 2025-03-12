SECTIONS {
  .text.main : { *(.text.main) }
  .text.foo : { *(.text.foo) }

  .data.main : { *(.data.dmain) }
  .data.foo : { *(.data.dfoo) }

  .bss.main : { *(.bss.bmain) }
  .bss.foo : { *(.bss.bfoo) }

  .comment : { *(.comment) }
}
