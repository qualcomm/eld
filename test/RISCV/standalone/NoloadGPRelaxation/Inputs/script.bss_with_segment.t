PHDRS {
  A PT_LOAD;
  BSS_SEGMENT PT_LOAD;
}

SECTIONS {
  .bss (NOLOAD) : { *(.*bss*) } :BSS_SEGMENT
  __global_pointer$ = .;
  .text : { *(.text*) } : A
}