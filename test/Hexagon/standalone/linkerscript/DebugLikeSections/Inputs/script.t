SECTIONS {
  .text : { *(.text) }
  .mystab  : { . = . +100; *(.stab) }
  .bss : { *(.bss) }
  .comment : { *(.comment) }
}
