SECTIONS {
  .text : { *(.text.*) }
  . = 0x8000;
  .empty : {}
  .bss : { *(.bss) }
}
