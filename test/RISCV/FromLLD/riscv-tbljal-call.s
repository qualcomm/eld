## Test that call/tail instructions are relaxed to cm.jt/cm.jalt when
## --relax-tbljal is enabled and the table jump is profitable.
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+zcmt %s -o %t.o
# RUN: %link %linkopts %t.o --relax-tbljal --defsym foo=0x150000 --defsym foo_1=0x150010 --defsym foo_3=0x150030 -o %t
# RUN: %link %linkopts %t.o --relax-tbljal --no-relax-tbljal --defsym foo=0x150000 --defsym foo_1=0x150010 --defsym foo_3=0x150030 -o %t.no
#
## Check disassembly for cm.jalt (rd=ra) and cm.jt (rd=zero).
# RUN: %objdump -d -M no-aliases --mattr=+zcmt --no-show-raw-insn %t | %filecheck --check-prefix=RV%xlen %s
# RUN: %objdump -d -M no-aliases --mattr=+zcmt --no-show-raw-insn %t.no | %filecheck --check-prefix=NO-TBLJAL %s
#
## Check jump table contents.
# RUN: %readelf -x .riscv.jvt %t | %filecheck --check-prefix=JVT%xlen %s
#
## 21 calls to foo become cm.jalt (RV32), tails become cm.jt.
# RV4-COUNT-21: cm.jalt
# RV4:         cm.jt
# RV4:         cm.jt
# RV4:         cm.jt
# RV4:         cm.jt
#
# RV8:         cm.jt
# RV8:         cm.jt
# RV8:         cm.jt
#
## Verify table entries contain the target addresses (little-endian).
# JVT4: 30001500 10001500 00001500
# JVT8: 30001500 00000000 10001500 00000000
#
# NO-TBLJAL-NOT: cm.jt
# NO-TBLJAL-NOT: cm.jalt

.global _start
.p2align 3
_start:
  .rept 21
  call foo
  .endr
  tail foo
  tail foo_1
  tail foo_1
  tail foo_1
  tail foo_3
  tail foo_2
  tail foo_3
  tail foo_3
  tail foo_3
  tail foo_3

foo_2:
  nop
