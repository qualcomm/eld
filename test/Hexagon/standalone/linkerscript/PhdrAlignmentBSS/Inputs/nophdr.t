SECTIONS {
.text : { *(.text) }
.a : { *(.bss.a) }
.b : { *(.bss.b) }
.c : { *(.bss.c) }
}
