SECTIONS {
  .foo : { *(*foo*) }
  . = . + 0x100;
  my_start = .;
    .debug_info : {
    start = .;
    (.debug_info)
    end = . ;
  }
  my_end = .;
  mytext : { *(*text*) }
}
