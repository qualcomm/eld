MEMORY {
  ram (rwx) : ORIGIN = 0x1000, LENGTH = 0x10000
  rom (rx)  : ORIGIN = 0x20000, LENGTH = 0x8000
}
SECTIONS {
  .text : { *(.text*) } > ram
}
