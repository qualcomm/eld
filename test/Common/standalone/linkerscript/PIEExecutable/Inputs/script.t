SECTIONS {
  .text : { *(.text*) }
  .rel.dyn : {
          __rel_dyn_start = .;
          *(.rel.dyn)
  }
  .rela.dyn : {
          __rela_dyn_start = .;
          *(.rela.dyn)
  }
  .dynamic : {
      _DYNAMIC_START = .;
      *(.dynamic)
  }

  /DISCARD/ : { *(.ARM.exidx*) }
}
