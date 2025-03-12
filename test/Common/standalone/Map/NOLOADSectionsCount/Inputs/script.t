SECTIONS {
  .bar (NOLOAD) : { *(*.bar) }
  .baz (NOLOAD) : { *(*.baz) }
  .text : { *(*.text*) }
}