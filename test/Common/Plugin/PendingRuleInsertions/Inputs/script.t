SECTIONS {
  foo: {
    *(*.foo)
  }
  bar: {
    *(*.bar)
  }
}

PLUGIN_OUTPUT_SECTION_ITER("CreateRules", "CreateRules")