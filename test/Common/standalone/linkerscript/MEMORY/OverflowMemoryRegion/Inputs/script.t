MEMORY {
  b1 (rx) : ORIGIN = 100, LENGTH = 30
}
SECTIONS {
  . = 0x2000;
  .t1 : {
            *(.text.foo)
    } >b1
  .t2 : {
            *(.text.bar)
    } >b1
  .t3 : {
            *(.text.baz)
    } >b1
  .t4 : {
            *(.text.far)
    } >b1

  /DISCARD/ : { *(.ARM*) }
}
