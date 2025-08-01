PHDRS {
  A PT_LOAD;
  B PT_LOAD;
  C PT_LOAD;
}

MEMORY {
  MEMA : ORIGIN = 0x100, LENGTH = 2000
  MEMB : ORIGIN = 0x800, LENGTH = 2000
  MEMC : ORIGIN = 0xc00, LENGTH = 2000
  LMEMA : ORIGIN = 100, LENGTH = 2000
  LMEMB : ORIGIN = 800, LENGTH = 2000
  LMEMC : ORIGIN = 1500, LENGTH = 2000
}

SECTIONS {
  .foo : ALIGN_WITH_INPUT  { *(.text.foo) } >MEMA AT>LMEMA :A
  .bar : ALIGN_WITH_INPUT  { *(.text.bar) } >MEMB AT>LMEMB :B
  .baz : ALIGN_WITH_INPUT { *(.text.baz) } >MEMC AT>LMEMC :C
  /DISCARD/ : { *(.ARM.exidx*) *(.riscv.attributes) }
}
