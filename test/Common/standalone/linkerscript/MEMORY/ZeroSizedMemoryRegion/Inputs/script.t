MEMORY {
  b1 (rx) : ORIGIN = 112, LENGTH = 100
  b2 (rx) : ORIGIN = 300, LENGTH = 100
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
  .t5 : {
            *(.text.nothing)
  } > b2

  /DISCARD/ : { *(.ARM*) }
}
