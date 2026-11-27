script_marker = 0x1234;
SECTIONS {
  .foo_out : { *(.text.foo) }
  .text : { *(.text*) }
}
