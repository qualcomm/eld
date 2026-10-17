## Verify compressed Xqccmi ILUT restrictions and permitted c.mv/c.add payloads.
#
# REQUIRES: riscv32
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+experimental-xqccmi,+xqciint %s -o %t.o
# RUN: %link %linkopts %t.o --no-relax --relax-ilut -o %t
# RUN: %readelf -x .riscv.ilut %t | %filecheck --check-prefix=HEX %s
# RUN: %objdump -d --section=.text -M no-aliases --mattr=+experimental-xqccmi,+xqciint --no-show-raw-insn %t | %filecheck --check-prefix=DISASM %s
#
## Only the c.mv and c.add pairs are legal and profitable ILUT payloads.
# HEX: 0x{{[0-9a-f]+}} 36863686 3e973e97
# DISASM-COUNT-6: c.j{{[ \t]}}
# DISASM-COUNT-6: c.jal
# DISASM-COUNT-6: c.beqz
# DISASM-COUNT-6: c.bnez
# DISASM-COUNT-6: c.jr
# DISASM-COUNT-6: c.jalr
# DISASM-COUNT-6: qc.c.mret
# DISASM-COUNT-6: qc.c.mnret
# DISASM-COUNT-6: qc.c.mileaveret
# DISASM-COUNT-3: qc.cm.ilut 0x0
# DISASM-COUNT-3: qc.cm.ilut 0x1
# DISASM-NOT: c.mv
# DISASM-NOT: c.add

.global _start
_start:
  ## PC-relative compressed jumps and branches are forbidden.
  .rept 6
  c.j 0
  .endr
  .rept 6
  c.jal 0
  .endr
  .rept 6
  c.beqz a0, 0
  .endr
  .rept 6
  c.bnez a1, 0
  .endr

  ## Register-indirect jumps and Xqci return forms are forbidden.
  .rept 6
  c.jr ra
  .endr
  .rept 6
  c.jalr ra
  .endr
  .rept 6
  qc.c.mret
  .endr
  .rept 6
  qc.c.mnret
  .endr
  .rept 6
  qc.c.mileaveret
  .endr

  ## These compressed ALU instructions do not reference PC and must be used.
  .rept 6
  c.mv a2, a3
  .endr
  .rept 6
  c.add a4, a5
  .endr
