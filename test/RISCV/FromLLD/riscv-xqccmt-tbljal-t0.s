## Xqccmt qc.cm.jalt can link through t0 (x5). The JVT entry uses bit 0 as
## metadata: 0 means ra (x1), and 1 means t0 (x5). The target address is still
## the table entry with bit 0 cleared by hardware.
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+experimental-xqccmt %s -o %t.o
# RUN: %link %linkopts %t.o --relax-tbljal=xqccmt --defsym foo=0x800000 -o %t
# RUN: %objdump -d -M no-aliases --mattr=+experimental-xqccmt --no-show-raw-insn %t \
# RUN:   | %filecheck --check-prefix=DISASM %s
# RUN: %readelf -x .riscv.jvt %t | %filecheck --check-prefix=JVT%xlen %s
#
# DISASM-COUNT-48: qc.cm.jalt
# DISASM-NOT: qc.cm.jt
#
## The same target appears twice in the call-table area. The plain value is for
## ra. The value with bit 0 set is for t0.
# JVT4: 00008000 01008000
# JVT8: 00008000 00000000 01008000 00000000

.global _start
.p2align 3
_start:
  .rept 24
  call foo
  call t0, foo
  .endr
