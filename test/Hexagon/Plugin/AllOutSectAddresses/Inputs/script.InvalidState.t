PLUGIN_OUTPUT_SECTION_ITER("FindOutSectAddresses", "InvalidStateFindOutSectAddresses")

SECTIONS {
  foo 0x8000 : { *(*foo) }
  bar 0x16000 : { *(*bar) }
}