PHDRS {
  FOO PT_LOAD;
  BAR PT_LOAD;
}

SECTIONS {
.bar (0x300) : {
                  *(.bar*)
                  . = . + 0x20;
                  *(.foo1)
                  *(.foo2)
                  *(.foo3)
                  *(.foo)
               } :BAR
}
