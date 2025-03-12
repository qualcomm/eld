SECTIONS {
  INCLUDE a.t
  INCLUDE f.t
  .text : { *(.text) }
}
