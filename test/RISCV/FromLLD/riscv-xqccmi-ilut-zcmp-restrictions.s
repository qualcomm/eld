## Verify Zcmp return instructions are not copied into an Xqccmi ILUT.
#
# REQUIRES: riscv32
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+experimental-xqccmi,+zcmp %s -o %t.o
# RUN: %link %linkopts %t.o --no-relax --relax-ilut -o %t
# RUN: %readelf -S %t | %filecheck --check-prefix=SECTIONS %s
# RUN: %objdump -d --section=.text -M no-aliases --mattr=+experimental-xqccmi,+zcmp --no-show-raw-insn %t | %filecheck --check-prefix=DISASM %s
#
# SECTIONS-NOT: .riscv.ilut
# DISASM-COUNT-6: cm.popret{{[ \t]}}
# DISASM-COUNT-6: cm.popretz

.global _start
_start:
  .rept 6
  cm.popret {ra}, 16
  .endr
  .rept 6
  cm.popretz {ra}, 16
  .endr
