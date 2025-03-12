SECTIONS {
  .foo : { *(.text.*) *(.ARM.exidx*)  }
}
