MEMORY {
  MYREGION (rwx) : ORIGIN = 0x100, LENGTH = 0x50
}

SECTIONS {
  text : { *(.text) } > MYREGION
  bss : ALIGN(0x40) { . = . + 100; } > MYREGION
}
