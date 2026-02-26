SECTIONS {
  .text : { *(.text*) }
  .data : { SORT(CONSTRUCTORS) }
}
