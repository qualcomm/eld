MEMORY {
  b1 (rx) : ORIGIN = 100, LENGTH = 48
  b2 (rx) : ORIGIN = 200, LENGTH = 48
  b3 (rw) : ORIGIN = 300, LENGTH = 48
}

PHDRS {
  A PT_LOAD;
  B PT_LOAD;
  C PT_LOAD;
  D PT_LOAD;
}

SECTIONS {
  . = 0x2000;
  .t1 : {
            *(.text.foo)
    } >b1 :A
  .t2 : {
            *(.text.bar)
    } >b2 :B
  .t3 : {
            *(.text.baz)
    } >b3 :C
  .t4 : {
            *(.text.far)
    } >b3 :D

  /DISCARD/ : { *(.ARM*) }
}
