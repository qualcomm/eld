SECTIONS {
  .text : { *(.text.txt) }
  .tdata : { *(.tdata*) }
  .tbss : { *(.tbss*) }
  __s_data = .;
  .bdata : { *(.bdata*) }
  __e_data = .;
  .xdata : { *(.data*) }
  .exidx : { *(.ARM.exidx*) }
}
