#define FOUR_INTS(z)                                                           \
  int *a##z = 0;                                                               \
  int **b##z = &a##z;                                                          \
  int ***c##z = &b##z;                                                         \
  int ****d##z = &c##z;
#define SIXTEEN_INTS(z)                                                        \
  FOUR_INTS(0##z) FOUR_INTS(1##z) FOUR_INTS(2##z) FOUR_INTS(3##z)
#define SIXTYFOUR_INTS(z)                                                      \
  SIXTEEN_INTS(0##z) SIXTEEN_INTS(1##z) SIXTEEN_INTS(2##z) SIXTEEN_INTS(3##z)
#define TWOFIFTYSIX_INTS(z)                                                    \
  SIXTYFOUR_INTS(0##z)                                                         \
  SIXTYFOUR_INTS(1##z) SIXTYFOUR_INTS(2##z) SIXTYFOUR_INTS(3##z)
#define THOUSAND_INTS(z)                                                       \
  TWOFIFTYSIX_INTS(0##z)                                                       \
  TWOFIFTYSIX_INTS(1##z) TWOFIFTYSIX_INTS(2##z) TWOFIFTYSIX_INTS(3##z)
#define FOURTHOUSAND_INTS(z)                                                   \
  THOUSAND_INTS(0##z)                                                          \
  THOUSAND_INTS(1##z) THOUSAND_INTS(2##z) THOUSAND_INTS(3##z)
#define SIXTEENTHOUSAND_INTS(z)                                                \
  FOURTHOUSAND_INTS(0##z)                                                      \
  FOURTHOUSAND_INTS(1##z) FOURTHOUSAND_INTS(2##z) FOURTHOUSAND_INTS(3##z)
#define SIXTYFOURTHOUSAND_INTS(z)                                              \
  SIXTEENTHOUSAND_INTS(0##z)                                                   \
  SIXTEENTHOUSAND_INTS(1##z) SIXTEENTHOUSAND_INTS(2##z)                        \
      SIXTEENTHOUSAND_INTS(3##z)
SIXTYFOURTHOUSAND_INTS(_test)
