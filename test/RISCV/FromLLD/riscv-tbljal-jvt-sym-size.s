## Test that the __jvt_base$ symbol size equals the jump vector table section size.
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+zcmt %s -o %t.o
# RUN: %link %linkopts %t.o --relax-tbljal --defsym foo=0x150000 -o %t
# RUN: %readelf -s %t | %filecheck --check-prefix=CHECK%xlen %s
#
## Marker symbol at JVT start marks the section as data.
# CHECK4:      0 NOTYPE  LOCAL  DEFAULT {{.*}} $d
# CHECK8:      0 NOTYPE  LOCAL  DEFAULT {{.*}} $d
#
## foo is the only JVT entry: 4 bytes (RV32) or 8 bytes (RV64).
# CHECK4:      4 NOTYPE  GLOBAL HIDDEN {{.*}} __jvt_base$
# CHECK8:      8 NOTYPE  GLOBAL HIDDEN {{.*}} __jvt_base$

.global _start
_start:
  .rept 4
  tail foo
  .endr
