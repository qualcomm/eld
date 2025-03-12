PHDRS
{
  /* Some SW tools assume the first segment is for debug info. Don't move INIT */
  INIT        PT_LOAD;
    ukernel_island   PT_LOAD;
    island_data   PT_LOAD;
    island_rodata   PT_LOAD;
    island_text   PT_LOAD;
    ukernel_main   PT_LOAD;
}

SECTIONS
{
  .qurtstart : {}
  .interp         : { *(.interp) }
  .note.gnu.build-id : { *(.note.gnu.build-id) }
  .hash           :  { *(.hash) }
  .gnu.hash       :  { *(.gnu.hash) }
  .dynsym         :  { *(.dynsym) }
  .dynstr         :  { *(.dynstr) }
  .gnu.version    :  { *(.gnu.version) }
  .gnu.version_d  :  { *(.gnu.version_d) }
  .gnu.version_r  :  { *(.gnu.version_r) }
  .rela.init      :  { *(.rela.init) }
  .rela.text      :  { *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*) }
  .rela.fini      :  { *(.rela.fini) }
  .rela.rodata    :  { *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*) }
  .rela.data.rel.ro   :  { *(.rela.data.rel.ro* .rela.gnu.linkonce.d.rel.ro.*) }
  .rela.data      :  { *(.rela.data .rela.data.* .rela.gnu.linkonce.d.*) }
  .rela.tdata	  :  { *(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*) }
  .rela.tbss	  :  { *(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*) }
  .rela.ctors     :  { *(.rela.ctors) }
  .rela.dtors     :  { *(.rela.dtors) }
  .rela.got       :  { *(.rela.got) }
  .rela.sdata     :  { *(.rela.sdata .rela.sdata.* .rela.gnu.linkonce.s.*) }
  .rela.sbss      :  { *(.rela.sbss .rela.sbss.* .rela.gnu.linkonce.sb.*) }
  .rela.sdata2    :  { *(.rela.sdata2 .rela.sdata2.* .rela.gnu.linkonce.s2.*) }
  .rela.sbss2     :  { *(.rela.sbss2 .rela.sbss2.* .rela.gnu.linkonce.sb2.*) }
  .rela.bss       :  { *(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*) }
  .rela.iplt      :
    {
      *(.rela.iplt)
    }
  .rela.plt       :
    {
      *(.rela.plt)
    }

   .rela.ukernel : { *(.rela.*.ukernel.*) }
 .ukernel.island : { *(*.ukernel.island) } : ukernel_island
  . = ALIGN(4K);
  .data.island : {
  } : island_data
  .data.IS.island :  {
   }
   .data.uDog.IS.island :  {
   }

  . = ALIGN(4K);
  .rodata.island : {
  } : island_rodata
  .text.island : {
  } : island_text
  .text.IS.island :  {
  }
  .text.uDog.IS.island :  {
  }

  .end.island : { } : island_text

  .BOOTUP : {} :INIT
  .start          :
  {
    KEEP (*(.start))
  }
  .CODE : {}
   .ukernel.main : { *(*.ukernel.main) } : ukernel_main  /* Defines .ukernel.main section */
  .init           :
  {
    KEEP (*(.init))
  }
  .plt            :   { *(.plt) }
  .iplt           :  { *(.iplt) }
  .text           : { *(.text.main) }
  .fini           :
  {
    *(.fini)
  }
  .pool.space : {*(.pool.space)}
}
