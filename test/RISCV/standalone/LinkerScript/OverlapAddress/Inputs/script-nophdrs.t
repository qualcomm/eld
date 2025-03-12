MEMORY {
  MYMEM(rwx) : ORIGIN = 0x1000, LENGTH = 0x8000
}

SECTIONS {
  .empty : {} >MYMEM
  .foo : {
           . = ALIGN(4K);
           *(.text.foo)
          . = . + 0x800;
  } > MYMEM
  .bar : {
      *(.text.bar)
  } > MYMEM
}
