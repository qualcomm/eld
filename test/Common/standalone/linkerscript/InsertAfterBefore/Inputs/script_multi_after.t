SECTIONS {
  .text.afterA : { *(.text.afterA) } INSERT AFTER .text.anchor
  .text.afterB : { *(.text.afterB) } INSERT AFTER .text.anchor
  .text.afterC : { *(.text.afterC) } INSERT AFTER .text.anchor
  .text.anchor : { *(.text.anchor) }
  .data : { *(.data) }
}
