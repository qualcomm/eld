MEMORY {
  RAM : ORIGIN = 0x1000, LENGTH = 1K
}
SECTIONS {
  .foo : { *(.text.foo) *(.text) } > RAM
  /DISCARD/ : { *(.riscv.attributes) *(.ARM.exidx) *(.hexagon.attributes) }
  my_start = .;
  .last_section : {
                   QUAD(100)
                   end_section = my_start + 100;
                  } > RAM
 .comment 0 : { *(.comment) }
  /DISCARD/ : { *(.note.GNU-stack) }
  .last_section1 :
  {
    LONG(0xA015E015)
  } > RAM
  .last_section2 :
  {
    LONG(0xB015E015)
  } > RAM
  .last_section3 :
  {
    LONG(0xC015E015)
  } > RAM
  .last_section4 :
  {
    LONG(0xD015E015)
  } > RAM
  .last_section5 :
  {
    LONG(0xE015E015)
  } > RAM
  _flash_used = LOADADDR(.last_section5) + SIZEOF(.last_section5);
}
