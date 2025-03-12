SECTIONS {
    .text.bar : {
        *(.text.bar)
    }
    . = 0x08000000;
    .text.foo : {
        *(.text.foo)
        *(.text.foo1)
    }
}