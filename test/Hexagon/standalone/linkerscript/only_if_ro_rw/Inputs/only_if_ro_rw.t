SECTIONS {
  .text.main : ONLY_IF_RO {
    *(.text.main_1)
    *(.text.main_2)
  }
  .text.main : ONLY_IF_RW {
    *(.text.main_1)
    *(.text.main_2)
  }
  .text : {
    *(.text*)
  }
  .data : ONLY_IF_RO {
    *(.data.*)
  }
  .sdata : ONLY_IF_RW {
    *(.data.*)
  }
}
