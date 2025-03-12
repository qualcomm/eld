MEMORY {
  b1 (rx) : ORIGIN = 100, LENGTH = 30
}
SECTIONS {
  . = 0x2000;
  .t1 : {
            *(.text.foo)
    } >b1
  .tbss : {
    *(.tbss*)
  } > b1
  /DISCARD/ : { *(.ARM*) *(.eh_*) }
}
