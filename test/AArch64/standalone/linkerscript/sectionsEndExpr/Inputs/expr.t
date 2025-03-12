SECTIONS {
  .section1 : {
  . = 1;
  sym1 = .;
  *(EXCLUDE_FILE(*1.o).text
     EXCLUDE_FILE(*lib2.a:)*data* )
  }
  .others : { *(*) }
  . = ALIGN(512);
  sym2 = .;
}
sym3 = 0x123;
