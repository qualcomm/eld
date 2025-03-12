PLUGIN_OUTPUT_SECTION_ITER("TargetChunkOffset","TARGETCHUNKOFFSET");
SECTIONS {
.foo : {
  *(.text.*)
}
.rodata : {
  *(.rodata*)
}
}
