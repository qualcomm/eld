SECTIONS {
  .foo : ONLY_IF_RW { *(.rw.foo) }
  .foo : ONLY_IF_RO { *(.ro.foo) }
}
