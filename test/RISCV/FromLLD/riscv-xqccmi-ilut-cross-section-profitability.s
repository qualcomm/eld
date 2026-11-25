## Patterns used in multiple code sections must share one profitability check.
#
# REQUIRES: riscv32
#
# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf \
# RUN:   -mattr=+relax,+experimental-xqccmi %s -o %t.o
# RUN: %link %linkopts %t.o --no-relax --relax-ilut -e _start -o %t
# RUN: %objdump -d --section=.text -M no-aliases \
# RUN:   --mattr=+experimental-xqccmi --no-show-raw-insn %t \
# RUN:   | %filecheck %s
#
## The two uses in _start alone do not cover the 4-byte entry cost. With the
## three matching uses in `three`, the shared pattern is profitable in both.
# CHECK-LABEL: <_start>:
# CHECK-NEXT: qc.cm.ilut
# CHECK-LABEL: <three>:
# CHECK-NEXT: qc.cm.ilut

.section .text.two,"ax",@progbits
.option push
.option norvc
.globl _start
_start:
  .rept 2
  lw a2, 0(a3)
  .endr
  ret
.option pop

.section .text.three,"ax",@progbits
.option push
.option norvc
.globl three
three:
  .rept 3
  lw a2, 0(a3)
  .endr
  ret
.option pop
