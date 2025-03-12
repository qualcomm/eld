MEMORY {
  a (rx) : ORIGIN = 0x100, LENGTH = 0x220
  b (rx) : ORIGIN = 0x100, LENGTH = 0x220
}
REGION_ALIAS("a", "b")

