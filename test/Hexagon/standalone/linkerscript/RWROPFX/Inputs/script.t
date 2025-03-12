SECTIONS {
  PF_X : { *(.text*) }
  R : {  *(.rodata*) }
  RW : {  *(.data*) }
}
