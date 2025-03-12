SECTIONS {
  .text 0x00000000 : { *(*text*) }
  .data 0x100000000 : { *(*data*) }
}
