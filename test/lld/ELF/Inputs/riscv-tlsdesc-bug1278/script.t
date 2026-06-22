SECTIONS {
  .text : { *(.text) }
  .data : {
    __global_pointer$ = .;
    *(.data)
  }
  .tbss : { *(.tbss) }
}
