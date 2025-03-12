MEMORY {
  b1 (rx) : ORIGIN = 100, LENGTH = 48
  b2 (rx) : ORIGIN = 200, LENGTH = 48
}
SECTIONS {
  .t1 : {
            *(.text.foo)
    } >b1
  .t2 : {
            *(.text.bar)
    } >b3
  /DISCARD/ : { *(.ARM*) }
}
