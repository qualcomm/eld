SECTIONS {
  .text : { *(.text*) }
  .tdata1 : { *(.tdata.[a]) }
  .data : { *(.data*) }
  .tdata2 : { *(.tdata.[bc]) }
  .tbss : { *(.tbss*) }
  .sdata : { *(.sdata* .sbss*) }
}
