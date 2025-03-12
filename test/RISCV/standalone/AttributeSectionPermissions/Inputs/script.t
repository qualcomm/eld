SECTIONS {
  text : { *(.text) }
  .line : { *(.line) }
  /DISCARD/ : { *(.someSection) }
  .riscv.attributes : { *(.riscv.attributes) }
}
