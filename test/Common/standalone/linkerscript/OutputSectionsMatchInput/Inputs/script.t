SECTIONS {
  .foo : { *(.foo.1) }
  .myfoo : { *(.foo) }
  .mybar : { *(.bar.1) }
}
