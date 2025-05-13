SECTIONS {
  .foo : { *(*foo*) }
  debug_info : {
    start = .;
    *(.debug_info*)
    end = .;
  }
  TEXT : { *(*text*) }
}
