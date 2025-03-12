SECTIONS {
  .mytext : { *2.o(.text.*) }
  .allotherfoo : { *(.text.foo) }
}
