SECTIONS {
  . = 0x40000;
  .text : {
      abs_inside_text_before_sect = ABSOLUTE(.);
      *(.text)
      abs_inside_text_after_sect = ABSOLUTE(.);
  }
  abs_text = ABSOLUTE(LOADADDR(.text));
  abs_dot = ABSOLUTE(.);
  .data : { *(.data) }
}
