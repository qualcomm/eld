# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf \
# RUN:   -mattr=+relax,+experimental-xqccmi,+xqcibi %s -o %t.o
# RUN: %link %linkopts %t.o --relax-ilut -o %t
# RUN: %objdump -d --section=.text -M no-aliases \
# RUN:   --mattr=+experimental-xqccmi,+xqcibi --no-show-raw-insn %t \
# RUN:   | %filecheck %s
#
# A forward Xqci 48-bit conditional branch skips repeated instructions and a
# 32-bit call. ILUT compaction must keep its target at the instruction after
# the call, not at the call's upper halfword.
#
# CHECK: qc.e.bgei a0, 0x58, {{.*}} <target>
# CHECK: qc.cm.ilut
# CHECK: <target>:
# CHECK-NEXT: ecall

.text
.globl _start
_start:
  qc.e.bgei a0, 88, target
  .option push
  .option norvc
  .rept 6
  addi a1, a1, 1
  .endr
  jal ra, after
  .option pop
target:
  ecall
after:
  ecall
