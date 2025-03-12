PHDRS {
  C PT_LOAD;
}
SECTIONS {
  .bss : {
     *(.bss*)
  } :C
  ASSERT(SIZEOF(:B) != 0, "bss is zero!")
}
