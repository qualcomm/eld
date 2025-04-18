SECTIONS {
  .data 0x0: {
    __global_pointer$ = . + 4;
     *(.sdata)
  }
  .text : { *(.text*) }
  . = ALIGN(4K);
}
