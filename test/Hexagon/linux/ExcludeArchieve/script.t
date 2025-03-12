SECTIONS {
  .text : {
     *(EXCLUDE_FILE(*lib2.a) .text*)
   }
  .mybar : { *(.text.bar)}
}
