## Test that --relax-ilut can create Xqccmi instruction lookup table
## instructions and materialize the mixed 64-bit/32-bit ILUT layout.
#
# REQUIRES: riscv32
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+experimental-xqccmi,+xqcilia %s -o %t.o
# RUN: %link %linkopts %t.o --relax-ilut -MapStyle txt -Map %t.map -o %t
# RUN: %link %linkopts %t.o --relax-ilut --no-relax-ilut -o %t.no
#
# RUN: %objdump -d --section=.text -M no-aliases --mattr=+experimental-xqccmi --no-show-raw-insn %t \
# RUN:   | %filecheck --check-prefix=DISASM %s
# RUN: %objdump -d --section=.text -M no-aliases --mattr=+experimental-xqccmi --no-show-raw-insn %t.no \
# RUN:   | %filecheck --check-prefix=NO-ILUT %s
# RUN: %readelf -x .riscv.ilut %t | %filecheck --check-prefix=HEX %s
# RUN: %readelf -s %t | %filecheck --check-prefix=SYMS %s
# RUN: %filecheck --check-prefix=MAP %s < %t.map
#
# DISASM-COUNT-5: qc.cm.ilut 0x0
# DISASM-COUNT-6: qc.cm.ilut 0x5
# DISASM-COUNT-3: qc.cm.ilut 0x2
# DISASM-COUNT-3: qc.cm.ilut 0x1
# DISASM-COUNT-3: qc.cm.ilut 0x3
# DISASM-COUNT-3: qc.cm.ilut 0x4
# DISASM-COUNT-3: jal zero,
# DISASM-NOT: addi a0, a0, 0x1
# DISASM-NOT: addi a1, a1, 0x2
#
# NO-ILUT-NOT: qc.cm.ilut
#
## The ILUT contains all legal entry forms: 32+32, 16+32, 16+48, 32+16,
## single 48, and 16+16. The 16+32, 32+16, and single-48 entries have
## trailing c.nop padding.
# HEX: 0x{{[0-9a-f]+}} 13051500 93852500 05079fb7 17800004
# HEX-NEXT: 0x{{[0-9a-f]+}} 05069386 36000100 13084800 85080100
# HEX-NEXT: 0x{{[0-9a-f]+}} 1f240200 10000100 01000100
#
## __ilut_base$ size is the table size. __ilut_dec$ is absolute and records the
## number of 64-bit entries before the 32-bit entries.
# SYMS-DAG: 44 NOTYPE  GLOBAL HIDDEN    {{.*}} __ilut_base$
# SYMS-DAG: 00000005     0 OBJECT  GLOBAL HIDDEN  ABS __ilut_dec$
#
# MAP: .riscv.ilut
# MAP: #{{[ \t]+}}.riscv.ilut entries (DEC=5):
# MAP: #{{[ \t]+}}[0] 1305150093852500
# MAP: #{{[ \t]+}}[1] 05079FB717800004
# MAP: #{{[ \t]+}}[2] 050693863600
# MAP: #{{[ \t]+}}[3] 130848008508
# MAP: #{{[ \t]+}}[4] 1F2402001000
# MAP: #{{[ \t]+}}[5] 01000100

.global _start
_start:
  .option push
  .option norvc
  .rept 5
  addi a0, a0, 1
  addi a1, a1, 2
  .endr
  .option pop

  .rept 6
  c.nop
  c.nop
  .endr

  ## 16+32: pad the 48-bit payload to a 64-bit entry with c.nop.
  .rept 3
  c.addi a2, 1
  .option push
  .option norvc
  addi a3, a3, 3
  .option pop
  jal zero, 0
  .endr

  ## 16+48: the pair fills a 64-bit entry exactly.
  .rept 3
  c.addi a4, 1
  qc.e.addi a5, a5, 1048577
  jal zero, 0
  .endr

  ## 32+16: pad the 48-bit payload to a 64-bit entry with c.nop.
  .rept 3
  .option push
  .option norvc
  addi a6, a6, 4
  .option pop
  c.addi a7, 1
  jal zero, 0
  .endr

  ## A single 48-bit instruction also requires c.nop padding.
  .rept 3
  qc.e.addai s0, 1048578
  jal zero, 0
  .endr

  .option push
  .option norvc
  .rept 3
  jal zero, 0
  .endr
  .option pop
