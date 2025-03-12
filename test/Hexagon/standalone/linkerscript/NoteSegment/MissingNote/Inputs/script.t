SECTIONS {
  .foo : { *(.text.foo) }
  . = 0xF0000000;
  .bar : { *(.text.bar) }
  .data : { *(.data*) }
  .note : { *(.note*) }
}
