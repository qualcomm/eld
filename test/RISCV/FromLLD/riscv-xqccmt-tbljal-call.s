## Test that --relax-tbljal=xqccmt can create Xqccmt table-jump instructions.
## Xqccmt uses the same encoding as Zcmt, but the disassembler prints the
## vendor mnemonics qc.cm.jt and qc.cm.jalt.
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+experimental-xqccmt %s -o %t.o
# RUN: %link %linkopts %t.o --relax-tbljal=xqccmt --defsym foo=0x800000 --defsym bar=0x900000 -o %t
# RUN: %objdump -d -M no-aliases --mattr=+experimental-xqccmt --no-show-raw-insn %t \
# RUN:   | %filecheck --check-prefix=JT %s
# RUN: %objdump -d -M no-aliases --mattr=+experimental-xqccmt --no-show-raw-insn %t \
# RUN:   | %filecheck --check-prefix=JALT %s
# RUN: %readelf -x .riscv.jvt %t | %filecheck --check-prefix=JVT%xlen %s

# RUN: %link %linkopts %t.o --relax-tbljal=XQCCMT --defsym foo=0x800000 --defsym bar=0x900000 -o %t.upper
# RUN: %objdump -d -M no-aliases --mattr=+experimental-xqccmt --no-show-raw-insn %t.upper \
# RUN:   | %filecheck --check-prefix=JT %s
# RUN: %objdump -d -M no-aliases --mattr=+experimental-xqccmt --no-show-raw-insn %t.upper \
# RUN:   | %filecheck --check-prefix=JALT %s
# RUN: %readelf -x .riscv.jvt %t.upper | %filecheck --check-prefix=JVT%xlen %s
#
# JT-COUNT-48: qc.cm.jt
# JALT-COUNT-48: qc.cm.jalt
#
## qc.cm.jt entries start at index 0. qc.cm.jalt entries start at index 32.
## The first non-zero word is the jump-table target. The later non-zero word is
## the call-table target.
# JVT4: 00008000
# JVT4: 00009000
# JVT8: 00008000 00000000
# JVT8: 00009000 00000000

.global _start
.p2align 3
_start:
  .rept 48
  tail foo
  call bar
  .endr
