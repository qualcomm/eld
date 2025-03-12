SECTIONS {
  .section1 : {
  // we exclude .text from all non archive relocatables
  // that end as 1.o
  *(EXCLUDE_FILE(*1.o).text EXCLUDE_FILE(*lib24.a:)*data* )
  // all data from lib24
  }
  .others : { *(*) }
}
