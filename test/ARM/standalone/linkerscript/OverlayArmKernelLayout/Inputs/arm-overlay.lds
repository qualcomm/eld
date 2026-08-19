OUTPUT_ARCH(arm)
ENTRY(_start)

SECTIONS
{
  . = 0xc1c00000;
  .text : { *(.text) }
  . = ALIGN(0x1000);

  __vectors_lma = .;
  OVERLAY 0xffff0000 : NOCROSSREFS AT(__vectors_lma)
  {
    .vectors { KEEP(*(.vectors)) }
    .vectors.bhb.loop8 { KEEP(*(.vectors.bhb.loop8)) }
    .vectors.bhb.bpiall { KEEP(*(.vectors.bhb.bpiall)) }
  }

  . = __vectors_lma + SIZEOF(.vectors) +
      SIZEOF(.vectors.bhb.loop8) + SIZEOF(.vectors.bhb.bpiall);

  __stubs_lma = .;
  .stubs ADDR(.vectors) + 0x1000 : AT(__stubs_lma)
  {
    *(.stubs)
  }

  . = __stubs_lma + SIZEOF(.stubs);
  . = ALIGN(8);
  .init.text : AT(ADDR(.init.text)) { *(.init.text) }
}
