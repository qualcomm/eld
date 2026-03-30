## Check that relaxation correctly adjusts symbol addresses and sizes.
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+zcmt %s -o %t.o
# RUN: %link %linkopts -Ttext=0x100000 --relax-tbljal %t.o -o %t.out
# RUN: %readelf -s %t.out | %filecheck --check-prefix=CHECK%xlen %s
#
# CHECK4:      00100000     4 NOTYPE  LOCAL  DEFAULT     1 a
# CHECK4-NEXT: 00100000     6 NOTYPE  LOCAL  DEFAULT     1 b
# CHECK4-NEXT: 00100000     0 NOTYPE  LOCAL  DEFAULT     1 $x
# CHECK4-NEXT: 00100004     2 NOTYPE  LOCAL  DEFAULT     1 c
# CHECK4-NEXT: 00100004     6 NOTYPE  LOCAL  DEFAULT     1 d
# CHECK4-NEXT: 00100000    10 NOTYPE  GLOBAL DEFAULT     1 _start
# CHECK4:                     NOTYPE  GLOBAL DEFAULT   {{.*}} __jvt_base$
#
# CHECK8:      00100000     4 NOTYPE  LOCAL  DEFAULT     1 a
# CHECK8-NEXT: 00100000     8 NOTYPE  LOCAL  DEFAULT     1 b
# CHECK8-NEXT: 00100000     0 NOTYPE  LOCAL  DEFAULT     1 $x
# CHECK8-NEXT: 00100004     4 NOTYPE  LOCAL  DEFAULT     1 c
# CHECK8-NEXT: 00100004     8 NOTYPE  LOCAL  DEFAULT     1 d
# CHECK8-NEXT: 00100000    12 NOTYPE  GLOBAL DEFAULT     1 _start
# CHECK8:                     NOTYPE  GLOBAL DEFAULT   {{.*}} __jvt_base$

.global _start
_start:
a:
b:
  add  a0, a1, a2
.size a, . - a
c:
d:
  call _start
.size b, . - b
.size c, . - c
  add a0, a1, a2
.size d, . - d
.size _start, . - _start
