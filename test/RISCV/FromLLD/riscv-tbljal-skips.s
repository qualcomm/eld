## Coverage for tbljal scanning skip cases.
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+zcmt %s -o %t.o
# RUN: %link %linkopts %t.o --relax-tbljal --defsym far=0x200000 -o %t
# RUN: %objdump -d --mattr=+zcmt --no-show-raw-insn %t | %filecheck %s
#
## Only the relaxable tails should become cm.jt.
# CHECK-COUNT-20: cm.jt
# CHECK-NOT:      cm.jalt

.global _start
.p2align 3
_start:
  .rept 20
  tail far
  .endr

.option push
.option norelax
  tail far
.option pop

.option push
.Ltmp:
  auipc t0, 0
  jalr  t0, t0, 0
.option relax
  .reloc .Ltmp, R_RISCV_CALL_PLT, far
  .reloc .Ltmp, R_RISCV_RELAX, 0
.option pop
