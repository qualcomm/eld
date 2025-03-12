PLUGIN_OUTPUT_SECTION_ITER("removechunks","REMOVECHUNKS");
SECTIONS
{
  .foobar:
  {
	*(.text.foo*)
	*(.text.bar*)
  }
}

