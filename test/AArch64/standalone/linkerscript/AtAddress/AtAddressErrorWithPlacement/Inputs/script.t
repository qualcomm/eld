PHDRS {
  FOO PT_LOAD;
  BAR PT_LOAD;
}

SECTIONS {
.foo : { *(.foo*) } :FOO
.bar (0x300) : {
                  *(.bar*)
                  . = 0x300 + 0x60;
               } :BAR
}
