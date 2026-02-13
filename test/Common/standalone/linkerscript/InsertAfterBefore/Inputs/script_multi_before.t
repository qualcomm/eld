SECTIONS {
  .text.beforeA : { *(.text.beforeA) } INSERT BEFORE .text.anchor
  .text.beforeB : { *(.text.beforeB) } INSERT BEFORE .text.anchor
  .text.anchor : { *(.text.anchor) }
  .data : { *(.data) }
}
