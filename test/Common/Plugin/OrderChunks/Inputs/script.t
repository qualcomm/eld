PLUGIN_OUTPUT_SECTION_ITER("OrderChunks", "ORDERBLOCKS");
SECTIONS
{
  .reordersection : {
    *(.text.three)
    *(.text.two)
    *(.text.one)
  }
}
