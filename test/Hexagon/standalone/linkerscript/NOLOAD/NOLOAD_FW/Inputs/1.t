PHDRS {
 A PT_LOAD;
}
SECTIONS {
 .f : {
   *(.text.foo)
 } :A
 .bss (NOLOAD) : {
   *(.bss*)
 } :A
 .bar : {
  *(.text.bar)
 } :A
}
