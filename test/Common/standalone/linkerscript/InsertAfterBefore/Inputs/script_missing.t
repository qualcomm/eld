SECTIONS {
  .text.after1 : { *(.text.after1) } INSERT AFTER .missing
  .text.anchor : { *(.text.anchor) }
}
