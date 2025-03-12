SECTIONS {
  .text.fns :
  {
    *(.text.fn)
    *lib2*:(  .text  EXCLUDE_FILE(*2.o) .text.* .gnu.linkonce.t.* )
  }
  .text.fn1 :
  {
    *(.text.fn1)
  }
}
