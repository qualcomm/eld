SECTIONS {
  FOO : { *(*foo*) } >A AT> B =0x1010
  BAR : { *(*bar*) } AT>C =0x1010 / 0x10
  BAZ : { *(*baz*) } = 0x1010 / 0x10
  /DISCARD/ : { *(*fred*) }
}