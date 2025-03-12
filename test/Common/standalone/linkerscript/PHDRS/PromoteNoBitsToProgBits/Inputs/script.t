PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .foo : { *(.text) *(.text.foo) } :A
  .b1 : { . = . + 100; } :A
  .b2 : { . = . + 100; } :A
  .bar : { *(.text.bar) } :A
  .b3 : { . = . + 1M;} : A
  .b4 : { . = . + 1M;} : A
}
