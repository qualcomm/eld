__my_image_start = 0x80000;
SECTIONS {
  INCLUDE bar.t
  INCLUDE_OPTIONAL tmp/bar.t
  INCLUDE_OPTIONAL doesnotexist.t
}
__my_image_end = 0x80000;