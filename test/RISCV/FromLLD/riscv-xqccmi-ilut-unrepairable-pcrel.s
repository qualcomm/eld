## Test that ILUT relaxation fails instead of committing edits that move an
## unrelocated PC-relative instruction.
#
# REQUIRES: riscv32
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+experimental-xqccmi,+xqcilia %s -o %t.o
# RUN: %not %link %linkopts %t.o --relax-ilut -o %t 2>&1 | %filecheck %s
#
# CHECK: Error in RISCV relaxation computation, unable to continue

.global _start
_start:
  .option push
  .option norvc
  .rept 3
  addi a0, a0, 1
  .endr
  auipc a1, 0
