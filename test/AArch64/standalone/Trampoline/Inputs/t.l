ENTRY(main)
SECTIONS {
  .text (0x12340000 + 0xE000) : {
    KEEP(* (.text))
  }
  .far (0x30000000): {
    KEEP(* (.text.*))
  } 
  /DISCARD/ : { *(.ARM.exidx*) *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) *(.init) *(.fini) }
}
