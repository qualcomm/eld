## Verify --relax-tbljal (Zcmt table-jump relaxation) is rejected for
## shared libraries and position independent executables.
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+zcmt %s -o %t.o
# RUN: %not %link %linkopts --relax-tbljal -shared %t.o -o %t.so 2>&1 \
# RUN:   | %filecheck %s
# RUN: %not %link %linkopts --relax-tbljal -pie %t.o -o %t.pie 2>&1 \
# RUN:   | %filecheck %s
# RUN: %not %link %linkopts --relax-tbljal --pic-executable %t.o \
# RUN:   -o %t.pic 2>&1 | %filecheck %s
#
# CHECK: Zcmt table jump relaxation is not supported for shared libraries
# CHECK-SAME: or position independent code

.text
.global _start
_start:
  nop
