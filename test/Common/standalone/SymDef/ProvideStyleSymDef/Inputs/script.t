SECTIONS {
  PROVIDE( a1 = 2000 );
  .foo : { *(.text.*) }
  .sdata : { *(.sdata.*) }
}