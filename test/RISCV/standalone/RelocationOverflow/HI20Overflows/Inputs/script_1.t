SECTIONS {
  .text 0x00000000 : { *(*text*) }
  .data 0xf0000000 : { *(*data*) }
}
