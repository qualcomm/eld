SECTIONS {
  .text : { *(.text) }
  .temp : { *(.temp) }
  __bss_start = .;
  . = ALIGN(1K);
  .bss : { *(.bss) }
}
