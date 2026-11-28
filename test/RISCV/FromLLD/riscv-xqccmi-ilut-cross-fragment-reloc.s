# REQUIRES: riscv32
# RUN: split-file %s %t.dir
# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf -mattr=+relax,+experimental-xqccmi %t.dir/first.s -o %t.first.o
# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf -mattr=+relax,+experimental-xqccmi %t.dir/second.s -o %t.second.o
# RUN: %link %linkopts %t.first.o %t.second.o --relax-ilut -o %t
# RUN: %objdump -d --section=.text -M no-aliases --mattr=+experimental-xqccmi --no-show-raw-insn %t | %filecheck %s
#
# The first input fragment is compacted by ILUT. The branch relocation in the
# second fragment must remain at its original fragment-local offset.
#
# CHECK: <second>:
# CHECK-NEXT: {{.*}}: addi t0, t0, 0x0
# CHECK-NEXT: {{.*}}: addi t0, t0, 0x0
# CHECK-NEXT: {{.*}}: bne a0, zero, {{.*}} <first>

#--- first.s
.option push
.option norvc
.text
.globl first
first:
  .rept 3
  lbu a2, 0(a1)
  .endr
.option pop

#--- second.s
.option push
.option norvc
.text
.globl second
second:
  .rept 2
  addi t0, t0, 0
  .endr
  bne a0, zero, .-8
  .reloc 8, R_RISCV_BRANCH, first
.option pop
