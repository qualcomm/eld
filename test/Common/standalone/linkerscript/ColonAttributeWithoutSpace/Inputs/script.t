SECTIONS {
  .foo1 0x1000 + 0x1000 ? 0x1000 : 3000 (PROGBITS) :ALIGN(4096) { *(.text.foo1) }
  .foo2 (0x1000 + 0x1000 ? 0x2000 : 3000) (PROGBITS) :ALIGN(4096) { *(.text.foo2) }
  .foo3 (0x1000 + 0x1000 ? 0x3000 : 3000) (PROGBITS) : ALIGN(4096) { *(.text.foo3) }
  .foo4 (0x1000 + 0x1000 ? 0x4000 : 3000) (PROGBITS) : ALIGN(4096) { *(.text.foo4) }
  .foo5 0x1000 + 0x1000 ? 0x5000 : 3000 (PROGBITS) :SUBALIGN(4096) { *(.text.foo5) }
  .foo6 (0x1000 + 0x1000 ? 0x6000 : 3000) (PROGBITS) :SUBALIGN(4096) { *(.text.foo6) }
  .foo7 (0x1000 + 0x1000 ? 0x7000 : 3000) (PROGBITS) : SUBALIGN(4096) { *(.text.foo7) }
  .foo8 (0x1000 + 0x1000 ? 0x8000 : 3000) (PROGBITS) : SUBALIGN(4096) { *(.text.foo8) }
  .foo9 0x1000 + 0x1000 ? 0x9000 : 3000 (PROGBITS) :ALIGN_WITH_INPUT { *(.text.foo9) }
  .foo10 (0x1000 + 0x1000 ? 0x10000 : 3000) (PROGBITS) :ALIGN_WITH_INPUT { *(.text.foo10) }
  .foo11 (0x1000 + 0x1000 ? 0x11000 : 3000) (PROGBITS) : ALIGN_WITH_INPUT { *(.text.foo11) }
  .foo12 (0x1000 + 0x1000 ? 0x12000 : 3000) (PROGBITS) : ALIGN_WITH_INPUT { *(.text.foo12) }
  .foo13 0x1000 + 0x1000 ? 0x13000 : 3000 (PROGBITS) :AT(0x13000) { *(.text.foo13) }
  .foo14 (0x1000 + 0x1000 ? 0x14000 : 3000) (PROGBITS) :AT(0x14000) { *(.text.foo14) }
  .foo15 (0x1000 + 0x1000 ? 0x15000 : 3000) (PROGBITS) : AT(0x15000) { *(.text.foo15) }
  .foo16 (0x1000 + 0x1000 ? 0x16000 : 3000) (PROGBITS) : AT(0x16000) { *(.text.foo16) }
}