SECTIONS {
.foo : { *(.foo) }
.bar (0x300) : { *(.bar) }
.baz : { *(.baz) }
.main : { *(.main) }
}
