SECTIONS {
  .foo : { *(.text.foo) }
  .bar : { *(.text.bar) }
  .baz : { *(.text.baz) }
  .mycommon : { *2.o*(COMMON) }
  .bss : { *(.bss*) }
}
