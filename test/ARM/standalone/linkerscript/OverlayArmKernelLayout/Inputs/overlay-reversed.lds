OUTPUT_ARCH(arm)
ENTRY(_start)

SECTIONS
{
  . = 0xc1c00000;
  .text : { *(.text) }
  . = ALIGN(0x1000);

  OVERLAY 0xffff0000 : AT(0xc1c00000)
  {
    .vectors.bhb.bpiall { KEEP(*(.vectors.bhb.bpiall)) }
    .vectors.bhb.loop8 { KEEP(*(.vectors.bhb.loop8)) }
    .vectors { KEEP(*(.vectors)) }
  }
}
