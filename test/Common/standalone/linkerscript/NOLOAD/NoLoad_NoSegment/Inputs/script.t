PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .foo (0x1000) (NOLOAD) : {
            *(.text.foo)
            . = . + 0x1000;
         }
  .bar (0x3000) : { *(.text.bar) } :A
  /DISCARD/ : { *(.ARM.exidx*) }
}
