SECTIONS {
.rodata : { *(.rodata.str*) *(.rodata*) }
.data : { *(.data*) }
}
