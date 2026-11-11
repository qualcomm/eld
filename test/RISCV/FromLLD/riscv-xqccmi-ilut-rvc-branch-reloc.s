# REQUIRES: riscv32
# RUN: split-file %s %t.dir
# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf -mattr=+relax,+experimental-xqccmi %t.dir/first.s -o %t.first.o
# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf -mattr=+relax,+experimental-xqccmi %t.dir/second.s -o %t.second.o
# RUN: %link %linkopts %t.first.o %t.second.o --relax-ilut -o %t
# RUN: %objdump -d --section=.text -M no-aliases --mattr=+experimental-xqccmi --no-show-raw-insn %t | %filecheck %s
#
# The branch is a compressed, relocation-bearing branch in a different input
# fragment from the ILUT deletions. Its relocation must remain at offset 8 in
# the second fragment and continue to target loop.
#
# CHECK: <second>:
# CHECK-NEXT: {{.*}}: c.nop
# CHECK-NEXT: {{.*}}: c.nop
# CHECK-NEXT: {{.*}}: c.nop
# CHECK: <loop>
# CHECK-NEXT: {{.*}}: c.nop
# CHECK-NEXT: {{.*}}: c.bnez a0, {{.*}} <loop>

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
.option rvc
.text
.globl second
second:
  .rept 3
  c.nop
  .endr
.globl loop
loop:
  c.nop
  c.bnez a0, .-2
  .reloc 8, R_RISCV_RVC_BRANCH, loop
.option pop
