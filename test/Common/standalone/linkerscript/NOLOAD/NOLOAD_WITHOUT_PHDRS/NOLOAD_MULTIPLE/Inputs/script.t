SECTIONS {
 .abss1 (NOLOAD) : {
   *(.bss.abss1)
 }
 .abss2 (NOLOAD) : {
   *(.bss.abss2)
 }
 .abss3 (NOLOAD) : {
   *(.bss.abss3)
 }
 .f (NOLOAD) : {
   *(.text.foo)
 }
 .bss1 (NOLOAD) : {
   *(.bss.bss1)
 }
 .bss2 (NOLOAD) : {
   *(.bss.bss2)
 }
 .bss3 (NOLOAD) : {
   *(.bss.bss3)
 }
 .bar (NOLOAD) : {
  *(.text.bar)
 }
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.attributes) *(.riscv.attributes) }
}
