SECTIONS {
  FOO : { *(*foo*) }
  BAR : {
    *(*bar*)
    . = ALIGN(16);
  } =0x1010
  /DISCARD/ : { *(*baz*) }
}