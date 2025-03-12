PLUGIN_OUTPUT_SECTION_ITER("OutputSectionContents", "OutputSectionContents");

SECTIONS {
  .rodata: {
    *(.nobits)
    *(.rodata*)
    FILL(97)
    . = . + 4;
    FILL(98)
    . = .+ 4;
    ASSERT(SIZEOF(.rodata) == 22, "Bad size")
  }
  .buffer : {. = . + 100; }
}