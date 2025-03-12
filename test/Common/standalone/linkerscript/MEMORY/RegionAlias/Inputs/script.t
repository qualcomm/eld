MEMORY {
  b1 (rx) : ORIGIN = 0x100, LENGTH = 0x220
  b2 (rx) : ORIGIN = 0x200, LENGTH = 0x200
  b3 (rx) : ORIGIN = 0x500, LENGTH = 0x100
  lmab1 (rx) : ORIGIN = 0x3000, LENGTH = 0x220
  lmab2 (rx) : ORIGIN = 0x6000, LENGTH = 0x200
  lmab3 (rx) : ORIGIN = 0x9000, LENGTH = 0x100
}
REGION_ALIAS("r1", "b1")
REGION_ALIAS("r2", "b2")
REGION_ALIAS("r3", "b3")
SECTIONS {
  .t1 : {
            *(.text.foo)
    } > b1 AT > lmab1
  .t2 : {
            *(.text.bar)
    } > b2 AT > lmab2
  .t3 : {
            *(.text.baz)
    } > b3 AT > lmab3
   /DISCARD/ : {
     *(.eh_frame*)
     *(.ARM.exidx*)
    }
}
