SECTIONS {
.a : { *(.data.*) }
.c : { *(.bss.c) }
.b : { *(.bss.b) }
size_of_a = SIZEOF(:A);
}
