MEMORY {
  b1 (!w!x) : ORIGIN = 0x100, LENGTH = 0x1000
  b2 (rx) : ORIGIN = 0x200, LENGTH = 0x1000
  b3 (rx) : ORIGIN = 0x300, LENGTH = 0x1000
}
SECTIONS {
  .t1 : {
      *(.rodata*)
    }
  .t2 : {
            *(.text.foo)
            *(.text.bar)
    }
  .t3 : {
            *(.text.baz)
    }
   /DISCARD/ : { *(.ARM.*) *(.eh_frame*) }
}
