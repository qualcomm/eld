# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf \
# RUN:   -mattr=+relax,+experimental-xqccmi,+xqcibi %s -o %t.o
# RUN: %link %linkopts %t.o --relax-ilut -o %t
# RUN: %objdump -d --section=.text -M no-aliases \
# RUN:   --mattr=+experimental-xqccmi,+xqcibi --no-show-raw-insn %t \
# RUN:   | %filecheck %s
# RUN: %readelf -r %t | %filecheck --check-prefix=RELOCS %s
#
# Three repeated loads are profitable ILUT candidates. The raw branches have no
# relocation and target the first load in each sequence. Each ILUT deletion
# moves the branch, so its immediate must be re-encoded as the code shrinks.
#
# CHECK: qc.cm.ilut
# CHECK: qc.cm.ilut
# CHECK: qc.cm.ilut
# CHECK: qc.bltui a2, {{(0x)?}}7, {{.*}} <_start>
# CHECK: qc.cm.ilut
# CHECK: qc.cm.ilut
# CHECK: qc.cm.ilut
# CHECK: qc.e.bnei a3, {{(0x)?}}7, {{.*}} <_start+0xa>

# RELOCS: There are no relocations in this file.

.text
.global _start
_start:
  lbu a2, 0(a1)
  lbu a2, 0(a1)
  lbu a2, 0(a1)
  # The numeric local target is resolved by the assembler, leaving no
  # relocation for the linker to repair after ILUT deletion.
  qc.bltui a2, 7, .-12

  lbu a3, 0(a1)
  lbu a3, 0(a1)
  lbu a3, 0(a1)
  qc.e.bnei a3, 7, .-12
