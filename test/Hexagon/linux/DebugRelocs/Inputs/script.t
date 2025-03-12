/* Script for -z combreloc: combine and sort reloc sections */
OUTPUT_FORMAT("elf32-littlehexagon", "elf32-bighexagon",
	      "elf32-littlehexagon")
OUTPUT_ARCH(hexagon)
SEARCH_DIR("/opt/codesourcery/hexagon/lib");
SECTIONS
{
  /* Read-only sections, merged into text segment: */
  PROVIDE (__executable_start = SEGMENT_START("text-segment", 0)); . = SEGMENT_START("text-segment", 0);
/* Start EBI memory. */
  .interp         :
                     { *(.interp) }
  .note.gnu.build-id :  { *(.note.gnu.build-id) }
  .hash           :  { *(.hash) }
  .gnu.hash       :  { *(.gnu.hash) }
  .dynsym         :  { *(.dynsym) }
  .dynstr         :  { *(.dynstr) }
  .gnu.version    :  { *(.gnu.version) }
  .gnu.version_d  :  { *(.gnu.version_d) }
  .gnu.version_r  :  { *(.gnu.version_r) }
  .rela.dyn       :
    {
      *(.rela.init)
      *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*)
      *(.rela.fini)
      *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*)
      *(.rela.data .rela.data.* .rela.gnu.linkonce.d.*)
      *(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*)
      *(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*)
      *(.rela.ctors)
      *(.rela.dtors)
      *(.rela.got)
      *(.rela.sdata .rela.lit[a48] .rela.sdata.* .rela.lit[a48].* .rela.gnu.linkonce.s.* .rela.gnu.linkonce.l[a48].*)
      *(.rela.sbss .rela.sbss.* .rela.gnu.linkonce.sb.*)
      *(.rela.sdata2 .rela.sdata2.* .rela.gnu.linkonce.s2.*)
      *(.rela.sbss2 .rela.sbss2.* .rela.gnu.linkonce.sb2.*)
      *(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*)
      PROVIDE_HIDDEN (__rela_iplt_start = .);
      *(.rela.iplt)
      PROVIDE_HIDDEN (__rela_iplt_end = .);
    }
  .rela.plt       :
    {
      *(.rela.plt)
    }
/* Code starts. */
  .start          :
  {
    KEEP (*(.start))
  } =0x00c0007f
  .init           :
  {
    KEEP (*(.init))
  } =0x00c0007f
  .plt            :  { *(.plt) }
  .iplt           :  { *(.iplt) }
  .text           :
  {
    *(.text.unlikely .text.*_unlikely)
    *(.text.hot .text.hot.* .gnu.linkonce.t.hot.*)
    *(.text .stub .text.* .gnu.linkonce.t.*)
    /* .gnu.warning sections are handled specially by elf32.em.  */
    *(.gnu.warning)
  } =0x00c0007f
  .fini           :
  {
    KEEP (*(.fini))
  } =0x00c0007f
  PROVIDE (__etext = .);
  PROVIDE (_etext = .);
  PROVIDE (etext = .);
/* Constants start. */
  .rodata         :
        {
          *(.rodata.hot .rodata.hot.* .gnu.linkonce.r.hot.*)
          *(.rodata .rodata.* .gnu.linkonce.r.*)
        }
  .rodata1        :  { *(.rodata1) }
  .eh_frame_hdr   :  { *(.eh_frame_hdr) }
  .eh_frame       :  ONLY_IF_RO { KEEP (*(.eh_frame)) }
  .gcc_except_table   :  ONLY_IF_RO { *(.gcc_except_table .gcc_except_table.*) }
/* Data start. */
  /* Adjust the address for the data segment.  We want to adjust up to
     the same address within the page on the next page up.  */
  . = ALIGN (CONSTANT (MAXPAGESIZE)) - ((CONSTANT (MAXPAGESIZE) - .) & (CONSTANT (MAXPAGESIZE) - 1)); . = DATA_SEGMENT_ALIGN (CONSTANT (MAXPAGESIZE), CONSTANT (COMMONPAGESIZE));
  /* Exception handling  */
  .eh_frame       :  ONLY_IF_RW { KEEP (*(.eh_frame)) }
  .gcc_except_table   :  ONLY_IF_RW { *(.gcc_except_table .gcc_except_table.*) }
  /* Thread Local Storage sections  */
  .tdata	  :  { *(.tdata .tdata.* .gnu.linkonce.td.*) }
  .tbss		  :  { *(.tbss .tbss.* .gnu.linkonce.tb.*) *(.tcommon) }
  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  }
  .init_array     :
  {
     PROVIDE_HIDDEN (__init_array_start = .);
     KEEP (*(SORT(.init_array.*)))
     KEEP (*(.init_array))
     PROVIDE_HIDDEN (__init_array_end = .);
  }
  .fini_array     :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(.fini_array))
    KEEP (*(SORT(.fini_array.*)))
    PROVIDE_HIDDEN (__fini_array_end = .);
  }
  .ctors          :
  {
    /* gcc uses crtbegin.o to find the start of
       the constructors, so we make sure it is
       first.  Because this is a wildcard, it
       doesn't matter if the user does not
       actually link against crtbegin.o; the
       linker won't look for a file to match a
       wildcard.  The wildcard also means that it
       doesn't matter which directory crtbegin.o
       is in.  */
    KEEP (*crtbegin.o(.ctors))
    KEEP (*crtbegin?.o(.ctors))
    /* We don't want to include the .ctor section from
       the crtend.o file until after the sorted ctors.
       The .ctor section from the crtend file contains the
       end of ctors marker and it must be last */
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
  .jcr            :  { KEEP (*(.jcr)) }
  .data.rel.ro    :  { *(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*) *(.data.rel.ro* .gnu.linkonce.d.rel.ro.*) }
  .dynamic        :  { *(.dynamic) }
  .got            :  { *(.got) *(.igot) }
  . = DATA_SEGMENT_RELRO_END (16, .);
  .got.plt        :  { *(.got.plt)  *(.igot.plt) }
  .data           :
  {
    *(.data.hot .data.hot.* .gnu.linkonce.d.hot.*)
    *(.data .data.* .gnu.linkonce.d.*)
    SORT(CONSTRUCTORS)
  }
  .data1          :  { *(.data1) }
  _edata = .; PROVIDE (edata = .);
  . = ALIGN (64);
  __bss_start = .;
  .bss            :
  {
   *(.dynbss)
   *(.bss.hot .bss.hot.* .gnu.linkonce.b.hot.*)
   *(.bss .bss.* .gnu.linkonce.b.*)
   *(COMMON)
   /* Align here to ensure that the .bss section occupies space up to
      _end.  Align after .bss to ensure correct alignment even if the
      .bss section disappears because there are no input sections. */
   . = ALIGN (. != 0 ? 64 : 1);
  }
  . = ALIGN (64);
  _end = .;
/* Small data start. */
  . = ALIGN (64);
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
  .got            :  { *(.got) *(.igot) }
  .sdata2         :
    {
      *(.sdata2 .sdata2.* .gnu.linkonce.s2.*)
    }
  .sbss2          :
  { *(.sbss2 .sbss2.* .gnu.linkonce.sb2.*) }
  . = ALIGN (64);
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
  PROVIDE (end = .);
  . = DATA_SEGMENT_END (.);
  /* Stabs debugging sections.  */
  .stab          0 :  { *(.stab) }
  .stabstr       0 :  { *(.stabstr) }
  .stab.excl     0 :  { *(.stab.excl) }
  .stab.exclstr  0 :  { *(.stab.exclstr) }
  .stab.index    0 :  { *(.stab.index) }
  .stab.indexstr 0 :  { *(.stab.indexstr) }
  .comment       0 :  { *(.comment) }
  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */
  /* DWARF 1 */
  .debug          0 :  { *(.debug) }
  .line           0 :  { *(.line) }
  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 :  { *(.debug_srcinfo) }
  .debug_sfnames  0 :  { *(.debug_sfnames) }
  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 :  { *(.debug_aranges) }
  .debug_pubnames 0 :  { *(.debug_pubnames) }
  /* DWARF 2 */
  .debug_info     0 :  { *(.debug_info .gnu.linkonce.wi.*) }
  .debug_abbrev   0 :  { *(.debug_abbrev) }
  .debug_line     0 :  { *(.debug_line) }
  .debug_frame    0 :  { *(.debug_frame) }
  .debug_str      0 :  { *(.debug_str) }
  .debug_loc      0 :  { *(.debug_loc) }
  .debug_macinfo  0 :  { *(.debug_macinfo) }
  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 :  { *(.debug_weaknames) }
  .debug_funcnames 0 :  { *(.debug_funcnames) }
  .debug_typenames 0 :  { *(.debug_typenames) }
  .debug_varnames  0 :  { *(.debug_varnames) }
  /* DWARF 3 */
  .debug_pubtypes 0 :  { *(.debug_pubtypes) }
  .debug_ranges   0 :  { *(.debug_ranges) }
  .gnu.attributes 0 :  { KEEP (*(.gnu.attributes)) }
  /DISCARD/       :  { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) }
}
