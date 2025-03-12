SECTIONS {
  .bss : { .  = . + 100; *(COMMON) *(.bss) }
}
