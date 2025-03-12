SECTIONS {
  .text : { *(.text.*) }
  /DISCARD/ : { *(.riscv.attributes*) }
}
