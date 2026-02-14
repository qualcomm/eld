SECTIONS {
.baz : { *(.baz) }
. = 0x100000 - 0x90;
.foo : { *(.foo) }
}
