SECTIONS {
  /DISCARD/ : { *(*f1*) }
  .text : { *(*text*) }
}

PLUGIN_SECTION_MATCHER("discardsections", "DiscardPluginSM");