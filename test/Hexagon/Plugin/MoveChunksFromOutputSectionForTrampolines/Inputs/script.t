PLUGIN_OUTPUT_SECTION_ITER("movechunksfromoutputsectionfortrampolines","MOVEOUTPUTCHUNKS");
SECTIONS
{
  .redistribute:
  {
	*(.text.hot*)
	*(.text.cold*)
  }
  .hot : {}
  . = 0xF0000000;
  .cold :  {}
  .unlikely : {}
}
