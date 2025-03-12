SECTIONS {
.text : {
     *(.text.foo)
     *(.rodata*) *(.text.bar)
} =0xaaaa
}
