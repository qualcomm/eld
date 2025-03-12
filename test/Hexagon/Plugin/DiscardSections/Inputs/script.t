PLUGIN_ITER_SECTIONS("discardsections", "DISCARD")

OUTPUT_FORMAT("elf32-littlehexagon", "elf32-bighexagon",
	      "elf32-littlehexagon")
OUTPUT_ARCH(hexagon)
ENTRY(start)
SECTIONS
{
  /* Code starts. */
  .start          :
  {
    KEEP (*(.start))
  } =0x00c0007f
  . = ALIGN(4K);
  .init           :
  {
    KEEP (*(.init))
  } =0x00c0007f

  .text.island :
  {
	*text_island.o(.text*)
  }
  .text_sw :
  {
	*text_sw.o(.text*)
  }

  .text           :
  {
    *(.text.unlikely .text.*_unlikely)
    *(.text.hot .text.hot.* .gnu.linkonce.t.hot.*)
    *(.text .stub .text.* .gnu.linkonce.t.*)
  } =0x00c0007f
  .fini           :
  {
    KEEP (*(.fini))
  } =0x00c0007f
  PROVIDE (__etext = .);
  PROVIDE (_etext = .);
  PROVIDE (etext = .);
  . = ALIGN(4K);
  /* Constants start. */

  .rodata.island :
  {
	*rodata_island.o(.rodata*)
  }

  .rodata         :
  {
    *(.rodata.hot .rodata.hot.* .gnu.linkonce.r.hot.*)
    *(.rodata .rodata.* .gnu.linkonce.r.*)
  }
  .eh_frame_hdr   :  { *(.eh_frame_hdr) }
  .eh_frame       :   { KEEP (*(.eh_frame)) }
  .gcc_except_table   :  { *(.gcc_except_table .gcc_except_table.*) }
  . = ALIGN(4K);
  .ctors          :
  {
    KEEP (*crtbegin.o(.ctors))
    KEEP (*crtbegin?.o(.ctors))
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o fini.o) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
  }
  .dtors          :
  {
    KEEP (*crtbegin.o(.dtors))
    KEEP (*crtbegin?.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o fini.o) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
  }


  .data.island :
  {
	*data_island.o(.data*)
  }

  .data           :
  {
    *(.data.hot .data.hot.* .gnu.linkonce.d.hot.*)
    *(.data .data.* .gnu.linkonce.d.*)
    SORT(CONSTRUCTORS)
  }
  _edata = .; PROVIDE (edata = .);
  . = ALIGN (64);
  /* Small data start. */
  . = ALIGN(4K);
  .sdata          :
  {
    PROVIDE (_SDA_BASE_ = .);
    *(.sdata.1 .sdata.1.* .gnu.linkonce.s.1.*)
    *(.sbss.1 .sbss.1.* .gnu.linkonce.sb.1.*)
    *(.scommon.1 .scommon.1.*)
    *(.sdata.2 .sdata.2.* .gnu.linkonce.s.2.*)
    *(.sbss.2 .sbss.2.* .gnu.linkonce.sb.2.*)
    *(.scommon.2 .scommon.2.*)
    *(.sdata.4 .sdata.4.* .gnu.linkonce.s.4.*)
    *(.sbss.4 .sbss.4.* .gnu.linkonce.sb.4.*)
    *(.scommon.4 .scommon.4.*)
    *(.lit[a4] .lit[a4].* .gnu.linkonce.l[a4].*)
    *(.sdata.8 .sdata.8.* .gnu.linkonce.s.8.*)
    *(.sbss.8 .sbss.8.* .gnu.linkonce.sb.8.*)
    *(.scommon.8 .scommon.8.*)
    *(.lit8 .lit8.* .gnu.linkonce.l8.*)
    *(.sdata.hot .sdata.hot.* .gnu.linkonce.s.hot.*)
    *(.sdata .sdata.* .gnu.linkonce.s.*)
  }
  .sbss           :
  {
    PROVIDE (__sbss_start = .);
    PROVIDE (___sbss_start = .);
    *(.dynsbss)
    *(.sbss.hot .sbss.hot.* .gnu.linkonce.sb.hot.*)
    *(.sbss .sbss.* .gnu.linkonce.sb.*)
    *(.scommon .scommon.*)
    . = ALIGN (. != 0 ? 64 : 1);
    PROVIDE (__sbss_end = .);
    PROVIDE (___sbss_end = .);
  }
  . = ALIGN (64);
  __bss_start = .;
  .bss            :
  {
   *(.dynbss)
   *(.bss.hot .bss.hot.* .gnu.linkonce.b.hot.*)
   *(.bss .bss.* .gnu.linkonce.b.*)
   *(COMMON)
  }
  . = ALIGN (64);
  _end = .;
  PROVIDE (end = .);
  .comment       0 :  { *(.comment) }
  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */
  /* DWARF 1 */
  .debug          0 :  { *(.debug) }
  .line           0 :  { *(.line) }
  .debug_aranges  0 :  { *(.debug_aranges) }
  .debug_pubnames 0 :  { *(.debug_pubnames) }
  /* DWARF 2 */
  .debug_info     0 :  { *(.debug_info .gnu.linkonce.wi.*) }
  .debug_abbrev   0 :  { *(.debug_abbrev) }
  .debug_line     0 :  { *(.debug_line) }
  .debug_frame    0 :  { *(.debug_frame) }
  .debug_str      0 :  { *(.debug_str) }
  .debug_loc      0 :  { *(.debug_loc) }
  /* DWARF 3 */
  .debug_pubtypes 0 :  { *(.debug_pubtypes) }
  .debug_ranges   0 :  { *(.debug_ranges) }
  /DISCARD/       :  { *(.note.GNU-stack) *(.gnu_debuglink) }
}
