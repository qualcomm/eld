MEMORY {
  b1 (rx) : ORIGIN = 100, LENGTH = 48
  b2 (rx) : ORIGIN = 200, LENGTH = 48
  b3 (rw) : ORIGIN = 300, LENGTH = 48
}
SECTIONS {
  . = 0x2000;
  .t1 : {
            *(.text.foo)
    } >b1
  .t2 : {
            *(.text.bar)
    } >b2
  .t3 : {
            *(.text.baz)
    } >b3
  .t4 : {
            *(.text.far)
    } >b3

  /DISCARD/ : { *(.ARM*) }
}
