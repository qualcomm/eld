SECTIONS {
  . = 0x10000;
  .text : { *(.text*) }
  .eh_a : { *1.o(.eh_frame) }
  .eh_b : { *2.o(.eh_frame) }
}
