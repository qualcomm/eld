__my_image_start = 0x80000;
SECTIONS {
  INCLUDE tmp/bar.t
  INCLUDE bar.t
}
__my_image_end = 0x80000;