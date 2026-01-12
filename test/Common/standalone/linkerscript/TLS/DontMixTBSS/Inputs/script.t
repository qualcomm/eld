MEMORY {
  RAM (rwx) : ORIGIN = 0x1000, LENGTH = 0x4000
}

SECTIONS {
  .text : { *(.text*) } >RAM
  .bss (NOLOAD) : ALIGN(0x1000) { *(.bss.empty_var2) *(.sbss.empty_var1) } >RAM
  .v (NOLOAD) : {
    *(.data.v)
    *(.sdata.v*)
    *(.sdata.4.v*)
    . = ALIGN(0x1000);
  } >RAM
  .tbssa : { *(.tbss.a) } >RAM
  .tbssb : { *(.tbss.b) } >RAM
  .tbssc : { *(.tbss.c) } >RAM
  .tbssd : { *(.tbss.d) } >RAM
  .tbsse : { *(.tbss.e) } >RAM
  .tbssf : { *(.tbss.f) } >RAM
  .w : { *(.data.w) *(.sdata.w*) *(.sdata.4.w*) } >RAM
  .empty_var2 (NOLOAD) : { *(.bss.empty_var1) *(.sbss.empty_var2) } >RAM
}
