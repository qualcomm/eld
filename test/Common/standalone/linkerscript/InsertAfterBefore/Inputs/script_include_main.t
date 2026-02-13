SECTIONS {
  INCLUDE "script_include_before.t"
  .text.anchor : { *(.text.anchor) }
  INCLUDE "script_include_after.t"
  .data : { *(.data) }
}
