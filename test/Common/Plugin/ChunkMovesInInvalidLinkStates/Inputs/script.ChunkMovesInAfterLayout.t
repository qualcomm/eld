SECTIONS {
  FOO: {
    *(*foo*)
  }
  BAR: {
    *(*bar*)
  }
}

PLUGIN_OUTPUT_SECTION_ITER("ChunkMovesInInvalidLinkStates", "ChunkMovesInAfterLayout")