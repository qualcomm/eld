PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .foo : { *(.text.foo) } :A
  . = ALIGN(10);
  .text . : { *(.text*) } :A
}

