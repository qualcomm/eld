# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf \
# RUN:   -mattr=+relax,+experimental-xqccmi,+xqcibi %s -o %t.o
# RUN: %link %linkopts %t.o --relax-ilut -o %t
# RUN: %objdump -d --section=.text -M no-aliases \
# RUN:   --mattr=+experimental-xqccmi,+xqcibi --no-show-raw-insn %t \
# RUN:   | %filecheck %s
#
# The branch targets the second instruction of a profitable repeated ILUT
# pair in another input section. That occurrence must remain in line; the
# other occurrences may use qc.cm.ilut.
#
# CHECK: <target>:
# CHECK-NEXT: lw a0, 0x124(s0)
# CHECK: qc.cm.ilut

.section .text.branch,"ax",@progbits
.globl _start
_start:
  qc.e.bgei a0, 88, target

.section .text.target,"ax",@progbits
.option push
.option norvc
  lw a2, 0x128(s0)
.globl target
target:
  lw a0, 0x124(s0)
  # These identical pairs keep the pattern profitable, but their second
  # instructions are not branch targets.
  lw a2, 0x128(s0)
  lw a0, 0x124(s0)
  lw a2, 0x128(s0)
  lw a0, 0x124(s0)
.option pop
  ecall
