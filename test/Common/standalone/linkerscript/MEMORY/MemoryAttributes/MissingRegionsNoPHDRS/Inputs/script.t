MEMORY {
  b1 (r!x) : ORIGIN = 0x100, LENGTH = 0x1000
  b2 (r!x) : ORIGIN = 0x200, LENGTH = 0x1000
  b3 (r!x) : ORIGIN = 0x300, LENGTH = 0x1000
}
SECTIONS {
  .t1 : {
            *(.text.foo)
    }
  .t2 : {
            *(.text.bar)
    }
  .t3 : {
            *(.text.baz)
    }
   /DISCARD/ : { *(.eh_frame*) }
}
