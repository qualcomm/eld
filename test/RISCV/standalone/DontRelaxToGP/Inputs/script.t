SECTIONS {
  .text : {
    *(.text)
  }
  .data : {
    *(.data)
  }
  . = 0x810;
  __global_pointer$ = .;
  .sdata : {
    *(.sdata)
  }
}
