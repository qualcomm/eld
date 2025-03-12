MEMORY {
  text1 (rx) : ORIGIN = 0x1000, LENGTH = 1000
  text2 (rx) : ORIGIN = 0x2000, LENGTH = 1000
}

SECTIONS {
  .foo : { *(.text.foo) } >text1 AT> text1
  .bar : { *(.*data*) } >text2 AT> text2
  /DISCARD/ : {
                *(.ARM.exidx)
                *(.text.boo)
                *(.eh_frame)
                *(.ARM.attributes)
                *(.riscv.attributes)
                *(.hexagon.attributes)
              }
  .empty : {} > text2 AT>text2
  .baz : { *(.text.baz) } >text1 AT> text1
  .comment : { *(.comment) }
}
