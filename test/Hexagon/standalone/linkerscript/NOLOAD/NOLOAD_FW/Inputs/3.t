PHDRS {
 A PT_LOAD;
}
SECTIONS {
 .abss1 (NOLOAD) : {
   *(.bss.abss1)
 } :A
 .abss2 (NOLOAD) : {
   *(.bss.abss2)
 } :A
 .abss3 (NOLOAD) : {
   *(.bss.abss3)
 } :A
 . = 0x80000000;
 .f : {
   *(.text.foo)
 } :A
 .bss1 (NOLOAD) : {
   *(.bss.bss1)
 } :A
 .bss2 (NOLOAD) : {
   *(.bss.bss2)
 } :A
 .bss3 (NOLOAD) : {
   *(.bss.bss3)
 } :A
 .bar : {
  *(.text.bar)
 } :A
}
