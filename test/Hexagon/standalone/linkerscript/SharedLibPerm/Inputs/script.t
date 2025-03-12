SECTIONS {
  .text :
  { *(*text*) }
  .rodata :
  { *(*rodata*) }

  .useless :
  { *(useless) }

  .others :
  { *(*) }
}
