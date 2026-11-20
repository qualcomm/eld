## Verify a relocation-bearing compressed branch after ordinary RVC
## relaxation and ILUT compaction.
#
# REQUIRES: riscv32
#
# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf \
# RUN:   -mattr=+relax,+experimental-xqccmi %s -o %t.o
# RUN: %link %linkopts %t.o --relax-ilut --image-base 0x08048000 -e _start -o %t
# RUN: %objdump -d --section=.text -M no-aliases --mattr=+experimental-xqccmi \
# RUN:   --no-show-raw-insn %t | %filecheck %s
#
# Ordinary RVC relaxation deletes bytes before the branch target, while ILUT
# deletes bytes between the target and source. Neither deletion may stale the
# branch relocation.
#
# CHECK: {{.*}}: c.mv t0, t0
# CHECK-NEXT: {{.*}}: c.mv t0, t0
# CHECK-NEXT: {{.*}}: c.mv t0, t0
# CHECK: <loop>:
# CHECK-NEXT: {{.*}}: c.addi a5, -0x1
# CHECK-NEXT: {{.*}}: qc.cm.ilut
# CHECK-NEXT: {{.*}}: qc.cm.ilut
# CHECK-NEXT: {{.*}}: qc.cm.ilut
# CHECK-NEXT: {{.*}}: c.bnez a5, {{.*}} <loop>

.option push
.option rvc
.text
.globl _start
_start:
  lui t0, %hi(__ilut_base$)
  addi t0, t0, %lo(__ilut_base$)
  csrw 0x800, t0
  li t0, 0
  csrw 0x801, t0
  addi a5, zero, 3
.rept 3
  addi t0, t0, 0
  .endr
.globl loop
loop:
  c.addi a5, -1
.option push
.option norvc
  .rept 3
  addi t0, t0, 0
  .endr
.option pop
  c.bnez a5, .-2
  .reloc 40, R_RISCV_RVC_BRANCH, loop
  addi a7, zero, 93
  ecall
.option pop
