SECTIONS {
  .text.fns :
  {
    *(.text.fn)
    /* Match the object file inside the archive, but not the filename */
    *2*:(  .text  EXCLUDE_FILE(*2.o) .text.* .gnu.linkonce.t.* )
  }
  .text.fn1 : {
    *(.text.fn1)
  }
}
