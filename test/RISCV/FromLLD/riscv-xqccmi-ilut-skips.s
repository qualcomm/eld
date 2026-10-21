## Test that --relax-ilut skips patterns that cannot legally be placed in the
## Xqccmi instruction lookup table.
#
# REQUIRES: riscv32
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+experimental-xqccmi %s -o %t.o
# RUN: %link %linkopts %t.o --no-relax --relax-ilut --defsym ext=0x1000 -o %t
# RUN: %readelf -S %t | %filecheck --check-prefix=SECTIONS %s
# RUN: %objdump -d --section=.text -M no-aliases --mattr=+experimental-xqccmi --no-show-raw-insn %t \
# RUN:   | %filecheck --check-prefix=DISASM %s
#
## The only repeated payloads are rejected: one is not profitable, one is
## PC-relative, one contains a relocation, and one is already qc.cm.ilut.
# SECTIONS-NOT: .riscv.ilut
# DISASM-COUNT-2: addi a0, a0, 0x1
# DISASM-COUNT-5: jal zero,
# DISASM-COUNT-5: lui a1,
# DISASM-COUNT-5: qc.cm.ilut 0x7

.global _start
_start:
  ## A repeated 32-bit instruction needs at least three uses to pay for its
  ## 4-byte table entry.
  .option push
  .option norvc
  .rept 2
  addi a0, a0, 1
  .endr
  .option pop

  ## JAL is PC-relative and must not be copied to the table even without an
  ## object relocation.
  .option push
  .option norvc
  .rept 5
  jal zero, 0
  .endr
  .option pop

  ## Relocated instructions must stay in place.
  .rept 5
  lui a1, %hi(ext)
  .endr

  ## Existing ILUT instructions must not recursively populate the table.
  .rept 5
  qc.cm.ilut 7
  .endr
