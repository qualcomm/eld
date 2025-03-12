SECTIONS {
  FOO: {
    *(*foo*)
  }
  BAR: {
    *(*bar*)
  }
}

PLUGIN_ITER_SECTIONS("ChunkMovesInInvalidLinkStates", "ChunkMovesInBeforeLayout")