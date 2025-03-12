PHDRS {
  FOO PT_LOAD;
  BAR PT_LOAD;
}

SECTIONS {
.bar (0x300) : {
                  *(.foo1)
                  . = . + 100;
                  *(.foo2)
                  *(.foo3)
                  *(.foo* .bar*)
               } :BAR
}
