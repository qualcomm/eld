## Ensure candidate savings account for near-call relaxation:
## near targets that can relax to shorter non-table-jump forms should not be
## preferred over far targets.
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+zcmt %s -o %t.o
# RUN: %link %linkopts %t.o --relax-tbljal --defsym near=0x90000 --defsym near_jal=0x90010 --defsym far=0x200000 -o %t
# RUN: %objdump -d -M no-aliases --mattr=+zcmt --no-show-raw-insn %t | %filecheck --check-prefix=RV%xlen %s
#
## Only the repeated far tails should become table jumps.
# RV4-COUNT-20: cm.jt
# RV4-NOT:      cm.jalt
# RV8-COUNT-20: cm.jt
# RV8-NOT:      cm.jalt

.global _start
.p2align 3
_start:
  .rept 20
  tail far
  .endr
  tail near
  jal zero, near_jal
  call near_call

.p2align 2
near_call:
  ret
