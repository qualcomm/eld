SECTIONS {
  foo : { *(*foo*) }
  bar : { *(*bar*) }
  /DISCARD/ : { *(*debug*) }
}