SECTIONS {
  .common : { *(COMMON .bss .bss*) }
  .smallcommon : { *(.scommon.*) }
}
