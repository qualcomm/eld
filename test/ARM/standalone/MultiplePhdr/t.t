PHDRS {
  A PT_LOAD;
  B PT_LOAD;
  REL PT_LOAD;
  DYN PT_DYNAMIC;
}
SECTIONS {
  .text : {
    *(.text .text.*)
   } : A
  .data(4096) : {
    KEEP(* (.data .bss COMMON))
  } : B
  .got : { *(.got.plt) *(.igot.plt) *(.got) *(.igot) }: REL
  .rel.dyn : { *(.rel.text) *(.rel.rodata) *(.rel.data) }

  .dynamic : {
    *(.dynamic)
  } : DYN :REL

  .gnu.attributes 0 : { KEEP (*(.gnu.attributes)) }
  .note.gnu.arm.ident 0 : { KEEP (*(.note.gnu.arm.ident)) }
  /DISCARD/ : { *(.hash) *(.dynsym) *(.dynstr) *(.ARM.exidx*) *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) *(.init) *(.fini) }
}
