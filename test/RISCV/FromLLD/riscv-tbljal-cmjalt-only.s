## Regression test: when cm.jt has zero net gain but cm.jalt is profitable,
## cm.jalt entries must still be emitted.
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+zcmt %s -o %t.o
# RUN: %link %linkopts %t.o --relax-tbljal --defsym foo=0x800000 --defsym bar=0x900000 -o %t
# RUN: %objdump -d -M no-aliases --mattr=+zcmt --no-show-raw-insn %t | %filecheck --check-prefix=DISASM %s
# RUN: %readelf -S %t | %filecheck --check-prefix=SEC%xlen %s
#
# DISASM-COUNT-48: cm.jalt
# DISASM-NOT: cm.jt
# SEC4: .riscv.jvt PROGBITS {{[0-9a-f]+}} {{[0-9a-f]+}} 000088
# SEC8: .riscv.jvt PROGBITS {{[0-9a-f]+}} {{[0-9a-f]+}} 000110

.global _start
.p2align 2
_start:
  .rept 24
  call foo
  call bar
  .endr
