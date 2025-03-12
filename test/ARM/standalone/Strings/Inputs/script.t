SECTIONS {
  .rodata : { *(.rodata*) }
  .nobits : { . = . + 4096; }
}
