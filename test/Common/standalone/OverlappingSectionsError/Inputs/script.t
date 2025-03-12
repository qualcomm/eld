SECTIONS {
  .foo : {
     *(.foo.bar)
     . = . + 0x100;
   }
   cur = .;
   /DISCARD/ : { *(.ARM.exidx*) }
}
