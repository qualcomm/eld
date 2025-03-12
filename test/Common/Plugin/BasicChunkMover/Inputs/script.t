SECTIONS {
  foo: {
    *(*.foo)
    . = . + 1000;
    *(*.foo_*)
  }
  bar: {
    *(*.bar)
    *(.bar*)
  }
}

PLUGIN_OUTPUT_SECTION_ITER("BasicChunkMover", "BasicChunkMover")