PHDRS {
  A PT_LOAD FILEHDR PHDRS;
  B PT_LOAD;
}

MEMORY {
  text1 (rx) : ORIGIN = 0xd000, LENGTH = 1000
  text2 (rx) : ORIGIN = 0xe000, LENGTH = 1000
}

SECTIONS {
   . = . + SIZEOF_HEADERS;
  .foo : {
           *(.text)
           *(.text.foo)
         } >text1 AT> text1 :A
  .bar : { *(.text.bar) } >text2 AT> text2 :B
  .baz : { *(.text.baz) } >text2 AT> text2 :B
  /DISCARD/ : { *(.*.attributes) *(.ARM.exidx*) }
  .comment : { *(.comment) }
}
