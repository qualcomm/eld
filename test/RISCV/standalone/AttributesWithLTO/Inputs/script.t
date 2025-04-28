SECTIONS {
  .text : { *(*text*) }
  .riscv.attributes 0 : { *(.riscv.attributes) }
}
