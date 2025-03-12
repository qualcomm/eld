SECTIONS {
  .text : {
    *(.text.foo) // IN1
    *(.text.bar) // IN2
   . = ALIGN(64);
   *(.text.b1)   // IN3
   . = ALIGN(128);
   *(.text.b2)  // IN4
   . = ALIGN(256);
   *(.text.b3)  // IN5
   . = ALIGN(512);
   *(.text.b4) // IN6
   . = ALIGN(1024);
  } = 0xa
}