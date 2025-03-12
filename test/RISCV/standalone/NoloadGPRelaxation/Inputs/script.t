PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .bss (NOLOAD) : { *(.*bss*) }
  __global_pointer$ = .;
  .text : { *(.text*) } : A
}