# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf \
# RUN:   -mattr=+relax,+experimental-xqccmi %s -o %t.o
# RUN: %link %linkopts %t.o --relax-ilut -o %t
# RUN: %objdump -d --section=.text -M no-aliases \
# RUN:   --mattr=+experimental-xqccmi --no-show-raw-insn %t \
# RUN:   | %filecheck %s
# RUN: %readelf -r %t | %filecheck --check-prefix=RELOCS %s
#
# Three repeated loads are profitable ILUT candidates. The raw branch has no
# relocation and targets the first load. Each ILUT deletion moves the branch,
# so its immediate must be re-encoded as the code shrinks.
#
# CHECK-COUNT-3: qc.cm.ilut
# CHECK: bne a2, zero, {{.*}} <_start>
# RELOCS: There are no relocations in this file.

.option push
.option norvc
.text
.global _start
_start:
  lbu a2, 0(a1)
  lbu a2, 0(a1)
  lbu a2, 0(a1)
  # The numeric local target is resolved by the assembler, leaving no
  # relocation for the linker to repair after ILUT deletion.
  bne a2, zero, .-12
.option pop
