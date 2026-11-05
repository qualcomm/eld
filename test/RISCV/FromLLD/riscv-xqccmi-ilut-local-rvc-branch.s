# REQUIRES: riscv32
# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf -mattr=+relax,+experimental-xqccmi %s -o %t.o
# RUN: %link %linkopts %t.o --relax-ilut -o %t
# RUN: %objdump -d --section=.text -M no-aliases --mattr=+experimental-xqccmi --no-show-raw-insn %t | %filecheck %s
#
# The local branch target moves when the preceding instructions are compacted
# into ILUT calls. The RVC branch relocation must follow both the source and
# the local target symbol.
#
# CHECK: <_start>:
# CHECK-COUNT-3: qc.cm.ilut
# CHECK: <loop_alias>:
# CHECK-NEXT: {{.*}}: c.nop
# CHECK-NEXT: {{.*}}: c.bnez a0, {{.*}} <loop_alias>

.text
.globl _start
_start:
.option push
.option norvc
  .rept 3
  lbu a2, 0(a1)
  .endr
.option pop

.option push
.option rvc
.globl loop_alias
loop_alias:
.Lloop:
  c.nop
  c.bnez a0, .-2
  .reloc 14, R_RISCV_RVC_BRANCH, .Lloop
.option pop
