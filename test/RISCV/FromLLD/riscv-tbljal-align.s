## This test checks that table-jump entries must track final symbol addresses even when
## later alignment relaxation changes those addresses.
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+zcmt %s -o %t.o
#
## Without relaxation, target stays at 0x2030.
# RUN: %link %linkopts --no-relax --section-start .text=0x2000 -e _start %t.o -o %t.no
# RUN: %objdump -d -M no-aliases --mattr=+zcmt --no-show-raw-insn %t.no \
# RUN:   | %filecheck --check-prefix=NORELAX %s
#
## With tbljal relaxation, CALL-pass converts tails to cm.jt first, then
## ALIGN-pass deletes extra padding and moves target to 0x2010.
# RUN: %link %linkopts --relax-tbljal --no-relax-c --section-start .text=0x2000 -e _start %t.o -o %t
# RUN: %objdump -d -M no-aliases --mattr=+zcmt --no-show-raw-insn %t \
# RUN:   | %filecheck --check-prefix=RELAX %s
# RUN: %readelf -x .riscv.jvt %t | %filecheck --check-prefix=JVT%xlen %s
#
# NORELAX: {{0*}}2030 <target>:
#
# RELAX-COUNT-4: cm.jt
# RELAX-NOT: cm.jalt
# RELAX: {{0*}}2010 <target>:
#
# JVT4: 10200000
# JVT8: 10200000 00000000

.text
.globl _start
.p2align 2
_start:
  .4byte 0x00000013 # addi x0, x0, 0
  tail target
  tail target
  tail target
  tail target
  .balign 16

.globl target
.p2align 2
target:
  ret
