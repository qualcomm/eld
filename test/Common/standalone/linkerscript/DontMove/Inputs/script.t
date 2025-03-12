SECTIONS {
  .text.foo : { DONTMOVE(*(.text.foo)) }
  .text.bar : { KEEP_DONTMOVE(*(.text.bar)) }
  .text.others : { *(.text.*) }
}
