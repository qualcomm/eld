SECTIONS {
  .text : { *(.text.foo) }
  /DISCARD/ : { *(.riscv.attributes) }
}
