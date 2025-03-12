SECTIONS
{
  . = 0x800;
  .interp          : { *(.interp) }
  .hash            : { *(.hash) }
  .dynamic         : { *(.dynamic) }
  .dynsym          : { *(.dynsym) }
  .dynstr          : { *(.dynstr) }
  .rela.dyn        : { *(.rela.dyn) }
  .text    (0x1000): { *(.text) }
  .plt     (0x1400): { *(.plt) }
  .data    (0x2000): { *(.data) }
  .got     (0x2400): { *(.got) }
  .got.plt (0x2600): { *(.got.plt) }
  .bss     (0x2800): { *(.bss) }
}
