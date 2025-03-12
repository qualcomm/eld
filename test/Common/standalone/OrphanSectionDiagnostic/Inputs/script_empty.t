SECTIONS {
    .foobar: {*(.text.foo*) *(.comment*) *(.note.GNU-stack*) *(.ARM.attributes*) *(.hexagon.attributes*)}
}