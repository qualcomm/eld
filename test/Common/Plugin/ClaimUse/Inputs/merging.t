SECTIONS {
  . = 0x10000;
  .text : { *(.text) }
  .merge.str : { *(.rodata.claim.str) }
  .merge.cst : { *(.rodata.claim.cst4) }
  .claimed : { *(.data.claimed) }
  .control : { *(.data.control) }
}
