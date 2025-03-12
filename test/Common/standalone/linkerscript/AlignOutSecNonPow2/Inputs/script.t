SECTIONS {
  text : ALIGN(30)  { *(.text) }
  zero : ALIGN(0) { QUAD(0); }
}