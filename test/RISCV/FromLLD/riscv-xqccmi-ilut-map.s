## Verify ILUT map output and linker script handling across gc-sections,
## explicit placement, and discard.
#
# REQUIRES: riscv32
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+experimental-xqccmi %s -o %t.o
#
## With --gc-sections only live code contributes ILUT entries.
# RUN: %link %linkopts %t.o --relax-ilut --gc-sections -e _start \
# RUN:   -MapStyle txt -Map %t.gc.map -o %t.gc
# RUN: %filecheck --check-prefix=GC %s < %t.gc.map
#
## Linker script placement should keep entries visible in map output.
# RUN: %echo "SECTIONS {" > %t.place.t
# RUN: %echo "  .text : { *(.text.start .text.live .text.dead) }" >> %t.place.t
# RUN: %echo "  .lut : { *(.riscv.ilut) }" >> %t.place.t
# RUN: %echo "}" >> %t.place.t
# RUN: %link %linkopts %t.o --relax-ilut -e _start \
# RUN:   -T %t.place.t -MapStyle txt -Map %t.place.map -o %t.place
# RUN: %filecheck --check-prefix=PLACE %s < %t.place.map
#
## Linker script discard of .riscv.ilut must prevent ILUT relaxation.
# RUN: %echo "SECTIONS {" > %t.discard.t
# RUN: %echo "  /DISCARD/ : { *(.riscv.ilut) }" >> %t.discard.t
# RUN: %echo "}" >> %t.discard.t
# RUN: %link %linkopts %t.o --relax-ilut -e _start \
# RUN:   -T %t.discard.t -MapStyle txt -Map %t.discard.map -o %t.discard
# RUN: %filecheck --check-prefix=DISCARD %s < %t.discard.map
# RUN: %objdump -d --section=.text -M no-aliases --mattr=+experimental-xqccmi \
# RUN:   --no-show-raw-insn %t.discard | %filecheck --check-prefix=DISCARD-DISASM %s
#
# GC: .riscv.ilut
# GC: #{{[ \t]+}}.riscv.ilut entries (DEC=1):
# GC: #{{[ \t]+}}[0] 1305150093852500
# GC-NOT: 1306360093864600
#
# PLACE: .lut
# PLACE: #{{[ \t]+}}.riscv.ilut entries (DEC=2):
# PLACE-DAG: #{{[ \t]+}}[0] 1305150093852500
# PLACE-DAG: #{{[ \t]+}}[1] 1306360093864600
#
# DISCARD: /DISCARD/
# DISCARD: *(.riscv.ilut)
# DISCARD-NOT: .riscv.ilut entries
# DISCARD-DISASM-NOT: qc.cm.ilut

.section .text.start, "ax", @progbits
.global _start
_start:
  j live

.section .text.live, "ax", @progbits
.global live
live:
  .option push
  .option norvc
  .rept 5
  addi a0, a0, 1
  addi a1, a1, 2
  .endr
  .option pop
  ret

.section .text.dead, "ax", @progbits
.global dead
dead:
  .option push
  .option norvc
  .rept 5
  addi a2, a2, 3
  addi a3, a3, 4
  .endr
  .option pop
  ret
