SECTIONS {
  .text : { *(.text) }
  foo : { *(*foo*) }
  bar : { *(*bar*) }
}