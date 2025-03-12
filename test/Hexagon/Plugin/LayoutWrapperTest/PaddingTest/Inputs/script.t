SECTIONS {
  .text : {
    . = . + 10;
    . = . + 20;
    *(.text.foo)
    . = . + 30;
    *(.text.baz)
    . = . + 40;
    *(.text.bar)
   . = ALIGN(64);
  } = 0xa
}

PLUGIN_OUTPUT_SECTION_ITER("PaddingTest", "PADDING");
