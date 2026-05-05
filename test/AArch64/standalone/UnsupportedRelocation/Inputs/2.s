  .section ".data","aw",%progbits
  .global foo
foo:
  .reloc foo, R_AARCH64_GOTREL64, bar
  .quad 0