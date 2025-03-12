SECTIONS {
  /DISCARD/ : { *(*foo*) }
  .text : { *(*text*) }
}

PLUGIN_SECTION_MATCHER("TestDiscardedSections", "DiscardedSections")