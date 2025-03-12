NOCROSSREFS(  .baz .text .bss .data .xsection .zsection)
SECTIONS {
  .zsection : { *(.data.z) }
  .xsection : { *(.data.x) }
  .baz : { *(.text.baz) }
  .text : { *(.text.*) }
  .bss : { *(.sdata*)}
}
