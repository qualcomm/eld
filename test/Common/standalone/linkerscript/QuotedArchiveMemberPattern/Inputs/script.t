SECTIONS {
  .foo : { "*lib1.a:*1.o"(.text*) }
  .text : { *(.text*) }
}
