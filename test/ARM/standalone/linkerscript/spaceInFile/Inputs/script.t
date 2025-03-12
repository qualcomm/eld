/*================================================================*/
/* Linker script.                                                 */
/*================================================================*/


/*================================================================*/
/* Configurable parameters                                        */
/*================================================================*/

__hw_demback_buffer_size__ = 2618496;

/*================================================================*/

PHDRS
{
  /* Some SW tools assume the first segment is for debug info. Don't move INIT.
     Keep these ordered by address, as per ELF spec, since PIL requires it. */
  INIT               PT_LOAD;

  TCM_STATIC         PT_LOAD; 

  TCM_OVERLAY_TEXT   PT_LOAD;
  TCM_OVERLAY_DATA   PT_LOAD;

  CODE               PT_LOAD; 
  CONST              PT_LOAD; 
  DATA               PT_LOAD; 
  BSS                PT_LOAD;

  DATA_L1WB_L2UC     PT_LOAD; 
  DATA_UNCACHED      PT_LOAD; 
}


SECTIONS
{
  /* Read-only sections, merged into text segment: */
  .interp         :
  { *(.interp) }
  .dynamic        :
  { *(.dynamic) }
  .hash           :  { *(.hash) }
  .dynsym         :  { *(.dynsym) }
  .dynstr         :  { *(.dynstr) }
  .gnu.version    :  { *(.gnu.version) }
  .gnu.version_d  :  { *(.gnu.version_d) }
  .gnu.version_r  :  { *(.gnu.version_r) }
  .rel.dyn        :
    {
      *(.rel.init)
      *(.rel.text .rel.text.* .rel.gnu.linkonce.t.*)
      *(.rel.fini)
      *(.rel.rodata .rel.rodata.* .rel.gnu.linkonce.r.*)
      *(.rel.data .rel.data.* .rel.gnu.linkonce.d.*)
      *(.rel.tdata .rel.tdata.* .rel.gnu.linkonce.td.*)
      *(.rel.tbss .rel.tbss.* .rel.gnu.linkonce.tb.*)
      *(.rel.ctors)
      *(.rel.dtors)
      *(.rel.got)
      *(.rel.sdata .rel.sdata.* .rel.gnu.linkonce.s.*)
      *(.rel.sbss .rel.sbss.* .rel.gnu.linkonce.sb.*)
      *(.rel.sdata2 .rel.sdata2.* .rel.gnu.linkonce.s2.*)
      *(.rel.sbss2 .rel.sbss2.* .rel.gnu.linkonce.sb2.*)
      *(.rel.bss .rel.bss.* .rel.gnu.linkonce.b.*)
    }
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
      *(.rela.sdata .rela.sdata.* .rela.gnu.linkonce.s.*)
      *(.rela.sbss .rela.sbss.* .rela.gnu.linkonce.sb.*)
      *(.rela.sdata2 .rela.sdata2.* .rela.gnu.linkonce.s2.*)
      *(.rela.sbss2 .rela.sbss2.* .rela.gnu.linkonce.sb2.*)
      *(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*)
      *(.rela.start)
      *(.rela.gcc_except_table)
      *(.rela.eh_frame)
    }
  .rel.plt        :  { *(.rel.plt) }
  .rela.plt       :  { *(.rela.plt) }

  /*------------------------------------------------------------------------
    Image start.
    ------------------------------------------------------------------------*/  
  . = ALIGN (DEFINED (TEXTALIGN)? (TEXTALIGN * 1K) : 4096);
  .BOOTUP : {} :INIT
  .start          :
  {
    KEEP (*(.start))
    . = ALIGN(8);
  } =0x00c0206c

  .CODE : {}
  .init           :
  {
    KEEP (*(.init))
  } =0x00c0206c
  .plt            :
  { *(.plt) }


  . = ALIGN (DEFINED (TEXTALIGN)? (TEXTALIGN * 1K) : 4096);
  .text :
  {
    *(.text.hot .text.hot.* .gnu.linkonce.t.hot.*)
    *(.text .stub .text.* .gnu.linkonce.t.*)
    /* .gnu.warning sections are handled specially by elf32.em.  */
    *(.gnu.warning)
  } :CODE =0x00c0206c

  .fini           :
  {
    KEEP (*(.fini))
  } =0x00c0206c

  PROVIDE (__etext = .);
  PROVIDE (_etext = .);
  PROVIDE (etext = .);

  . = ALIGN (DEFINED (RODATAALIGN)? (RODATAALIGN * 1K) : 4096);
  .CONST : {} :CONST

  .rodata         :
  {

    *(.rodata.hot .rodata.hot.* .gnu.linkonce.r.hot.*)
    *(.rodata .rodata.* .gnu.linkonce.r.*)
  }
  .rodata1        :  { *(.rodata1) }
  .sdata2         :
  { *(.sdata2 .sdata2.* .gnu.linkonce.s2.*) }
  .sbss2          :
  { *(.sbss2 .sbss2.* .gnu.linkonce.sb2.*) }
  .eh_frame_hdr :  { *(.eh_frame_hdr) }


  /* Adjust the address for the data segment.  We want to adjust up to
     the same address within the page on the next page up.  */
  . = ALIGN (4096) + (. & (4096 - 1));
  . = ALIGN (DEFINED (DATAALIGN)? (DATAALIGN * 1K) : 4096);
  .DATA : {} :DATA
  /* Ensure the __preinit_array_start label is properly aligned.  We
     could instead move the label definition inside the section, but
     the linker would then create the section even if it turns out to
     be empty, which is not pretty. */
  . = ALIGN (64);
  PROVIDE (__preinit_array_start = .);
  .preinit_array     :  { *(.preinit_array) }
  PROVIDE (__preinit_array_end = .);
  PROVIDE (__init_array_start = .);
  .init_array     :  { *(.init_array) }
  PROVIDE (__init_array_end = .);
  PROVIDE (__fini_array_start = .);
  .fini_array     :  { *(.fini_array) }
  PROVIDE (__fini_array_end = .);


  . = ALIGN (4K);
  __bss_start = .;  
  .bss   (NOLOAD)    :
  {

    *(.bss.hot .bss.hot.* .gnu.linkonce.b.hot.*)
    *(.bss .bss.* .gnu.linkonce.b.*)
    *(.dynbss)
    *(COMMON)
    /* Align here to ensure that the .bss section occupies space up to
        _end.  Align after .bss to ensure correct alignment even if the
        .bss section disappears because there are no input sections.  */
    . = ALIGN (64);
  } :BSS

  . = ALIGN (64);
  .demback_offoad_bss :
  {
    __hw_demback_buffer_start__ = .;
    . += __hw_demback_buffer_size__;  /* Allocate memory */
    . = ALIGN(64);
    __hw_demback_buffer_end__ = .;
  } :BSS


  _end = .;

  /* We don't support .sbss and .bss in G0 builds.
     crt0 needs a start location for GP register; park it in an invalid memory
     location so usage will trigger a page fault. 
     Input files still generate empty .sdata segments - capture them in a dummy
     segment and ensure it is empty.
     */
  PROVIDE( _SDA_BASE_ = 0); 
  PROVIDE( __sbss_start = 0);
  PROVIDE( ___sbss_start = 0);
  PROVIDE( __sbss_end = 0);
  PROVIDE( ___sbss_end = 0);

  .dummy_sda          :
  {
    __dummy_sda_base__ = . ;
    *(.sdata.1 .sdata.1.* .gnu.linkonce.s.1.*)
    *(.sbss.1 .sbss.1.* .gnu.linkonce.sb.1.*)
    *(.scommon.1 .scommon.1.*)
    *(.sdata.2 .sdata.2.* .gnu.linkonce.s.2.*)
    *(.sbss.2 .sbss.2.* .gnu.linkonce.sb.2.*)
    *(.scommon.2 .scommon.2.*)
    *(.sbss.4 .sbss.4.* .gnu.linkonce.sb.4.*)
    *(.scommon.4 .scommon.4.*) 
    *(.sdata.4 .sdata.4.* .gnu.linkonce.s.4.*)
    *(.lita .lita.* .gnu.linkonce.la.*)
    *(.lit4 .lit4.* .gnu.linkonce.l4.*)
    *(.lit8 .lit8.* .gnu.linkonce.l8.*)
    *(.sdata.8 .sdata.8.* .gnu.linkonce.s.8.*)
    *(.sbss.8 .sbss.8.* .gnu.linkonce.sb.8.*)
    *(.scommon.8 .scommon.8.*)
    *(.sdata.hot .sdata.hot.* .gnu.linkonce.s.hot.*)
    *(.sdata .sdata.* .gnu.linkonce.s.*) 

    *(.sbss.hot .sbss.hot.* .gnu.linkonce.sb.hot.*)
    *(.sbss .sbss.* .gnu.linkonce.sb.*)
    *(.scommon .scommon.*)
    *(.dynsbss)
    __dummy_sda_end__ = .;
  }
 
  PROVIDE (end = .);

/* Stabs debugging sections.  */
  .stab          0 : { *(.stab) }
  .stabstr       0 : { *(.stabstr) }
  .stab.excl     0 : { *(.stab.excl) }
  .stab.exclstr  0 : { *(.stab.exclstr) }
  .stab.index    0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment       0 : { *(.comment) }
/* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */
  /* DWARF 1 */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }
  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }
  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }
  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }
  /* DWARF 2.1 */
  .debug_ranges   0 : { *(.debug_ranges) }
  /* DWARF 3 */
  .debug_pubtypes 0 : { *(.debug_pubtypes) }

}

