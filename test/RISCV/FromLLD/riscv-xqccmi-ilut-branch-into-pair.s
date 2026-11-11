# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf \
# RUN:   -mattr=+relax,+experimental-xqccmi %s -o %t.o
# RUN: %link %linkopts %t.o --relax-ilut -o %t
# RUN: %objdump -d --section=.text -M no-aliases \
# RUN:   --mattr=+experimental-xqccmi --no-show-raw-insn %t \
# RUN:   | %filecheck %s
#
# The branch enters the second instruction of a repeated two-instruction
# candidate. That occurrence must not be replaced by qc.cm.ilut: the ILUT
# instruction represents the whole pair and cannot provide an entry point at
# the second source instruction.
#
# CHECK: <target>:
# CHECK-NEXT: {{.*}}: c.addi a1, 0x1
# CHECK-COUNT-3: qc.cm.ilut
# CHECK: c.beqz a1, {{.*}} <target+0xe>

.option push
.option rvc
.text
.globl _start
_start:
.option push
.option norvc
  addi a0, a0, 1
.option pop
  c.addi a1, 1
.option push
.option norvc
  addi a0, a0, 1
.option pop
.globl target
target:
  c.addi a1, 1
.option push
.option norvc
  addi a0, a0, 1
.option pop
  c.addi a1, 1
.option push
.option norvc
  addi a0, a0, 1
.option pop
  c.addi a1, 1
.option push
.option norvc
  addi a0, a0, 1
.option pop
  c.addi a1, 1
  bne a1, zero, target
.option pop
