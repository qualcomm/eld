SECTIONS {
  .text : { *(.text*) }
  . = ALIGN(4K);
  .data : {
    __global_pointer$ = .;
     *(*data*)
  }
}
