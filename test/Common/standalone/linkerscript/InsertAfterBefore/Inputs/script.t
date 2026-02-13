SECTIONS {
  .text.after1 : { *(.text.after1) } INSERT AFTER .text.anchor
  .text.before : { *(.text.before) } INSERT BEFORE .text.anchor
  .text.anchor : { *(.text.anchor) }
  .text.after2 : { *(.text.after2) } INSERT AFTER .text.anchor
  .data : { *(.data) }
}
