## Verify map output uses the Xqccmt instruction names based on the
## --relax-tbljal=xqccmt mode, not input mapping symbols or attributes.
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+zcmt %s -o %t.o
# RUN: %link %linkopts %t.o --relax-tbljal=xqccmt --defsym foo=0x800000 \
# RUN:   -MapStyle txt -Map %t.map -o %t
# RUN: %filecheck %s < %t.map
#
# CHECK: .riscv.jvt
# CHECK: #{{[ \t]+}}.riscv.jvt entries:
# CHECK: qc.cm.jt{{[ \t]+}}foo{{[ \t]+}}0x800000
# CHECK: qc.cm.jalt{{[ \t]+}}foo{{[ \t]+}}0x800000

.global _start
.p2align 3
_start:
  .rept 48
  tail foo
  call foo
  .endr
