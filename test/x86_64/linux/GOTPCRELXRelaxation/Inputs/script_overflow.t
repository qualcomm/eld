SECTIONS {
    . = 0x2000;
    .text : { *(.text*) }
    . = 0x100020000;
    .data : { *(.data*) *(.bss*) }
}
