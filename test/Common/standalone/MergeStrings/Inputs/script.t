SECTIONS {
  .out : {
    *1.o*(.rodata.*)
    *2.o*(.rodata.*)
  }
}