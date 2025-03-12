SECTIONS {
  .foo : { *(.text.foo) }
  . = ALIGN(10);
  .text . : { *(.text*) }
}

