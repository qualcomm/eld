## When there are too few calls, table jump relaxation should not be profitable.
## Verify the .riscv.jvt section has zero size and no cm.jt/cm.jalt are emitted.
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+zcmt %s -o %t.o
# RUN: %link %linkopts %t.o --relax-tbljal --defsym foo=0x150000 -o %t
# RUN: %readelf -S %t | %filecheck --check-prefix=SEC %s
# RUN: %objdump -d --mattr=+zcmt --no-show-raw-insn %t | %filecheck --check-prefix=DISASM %s
#
# SEC: .riscv.jvt PROGBITS {{[0-9a-f]+}} {{[0-9a-f]+}} 000000
#
# DISASM-NOT: cm.jt
# DISASM-NOT: cm.jalt

.global _start
_start:
  call foo
  tail foo_1
  tail foo_2
  tail foo_3

foo_1:
  nop
foo_2:
  nop
foo_3:
  nop
