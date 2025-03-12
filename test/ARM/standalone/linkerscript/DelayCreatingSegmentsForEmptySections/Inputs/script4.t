SECTIONS {
  .text : { *(.text.*) }
  .bss : { *(.bss) }
  . = 0x8000;
  .empty : {}
}
