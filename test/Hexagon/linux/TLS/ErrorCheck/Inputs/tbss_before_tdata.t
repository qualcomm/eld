SECTIONS {
  .text : { *(.text*) }
  .tdata1 : { *(.tdata.[a]) }
  .tdata2 : { *(.tdata.[b]) }
  .tbss : { *(.tbss*) }
  .tdata3 : { *(.tdata.[c]) }
  .data : { *(.data*) }
  .sdata : { *(.sdata* .sbss*) }
}
