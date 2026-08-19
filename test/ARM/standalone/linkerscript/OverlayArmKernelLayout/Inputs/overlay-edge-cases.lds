OUTPUT_ARCH(arm)
ENTRY(_start)

SECTIONS
{
  . = 0xc1c00000;
  .text : { *(.text) }
  . = ALIGN(0x1000);

  OVERLAY : AT(0xc1c01000)
  {
    .empty { }
    .vectors { KEEP(*(.vectors)) }
    .vectors.bhb.loop8 { KEEP(*(.vectors.bhb.loop8)) }
    .vectors.bhb.bpiall { KEEP(*(.vectors.bhb.bpiall)) }
  }
}
