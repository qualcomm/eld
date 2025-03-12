PHDRS {
  A PT_LOAD;
}

MEMORY {
  MYMEM(rwx) : ORIGIN = 0x1000, LENGTH = 0x8000
}

SECTIONS {
  .empty : {} >MYMEM :A
  .foo : {
           . = ALIGN(4K);
           *(.text.foo)
          . = . + 0x800;
  } > MYMEM :A
  .bar : {
      *(.text.bar)
  } > MYMEM :A
}
