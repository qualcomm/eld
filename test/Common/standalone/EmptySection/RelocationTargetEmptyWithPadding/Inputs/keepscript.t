SECTIONS {
    . = 0x10003;
    .baz : {
        *(.text.baz1)
        . = . + 16;
        KEEP(*(.text.baz))
        . = . + 28;
        *(.text.baz2)
        . = . + 40;
    }
    .foo : {
        *(.text.foo)
        *(.text*)
    }
}