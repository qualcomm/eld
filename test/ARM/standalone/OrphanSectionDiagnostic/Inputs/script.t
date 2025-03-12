SECTIONS {
    .foobar : { *(.text.foo*) }
    /DISCARD/ : { *(.text.baz) }
}
