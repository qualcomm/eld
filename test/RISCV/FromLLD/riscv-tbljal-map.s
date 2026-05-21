## Verify table-jump map output entries across partial links, gc-sections,
## linker script placement, and linker script discard.
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+zcmt %s -o %t.o
#
## Partial link (-r) must not create a table-jump section or entries.
# RUN: %link %linkopts %t.o --relax-tbljal -r -MapStyle txt -Map %t.r.map -o %t.r.o
# RUN: %filecheck --check-prefix=PARTIAL %s < %t.r.map
#
## With --gc-sections only live code contributes table-jump entries.
# RUN: %link %linkopts %t.o --relax-tbljal --gc-sections -e _start \
# RUN:   --defsym live=0x150000 --defsym dead=0x160000 \
# RUN:   -MapStyle txt -Map %t.gc.map -o %t.gc
# RUN: %filecheck --check-prefix=GC %s < %t.gc.map
#
## Linker script placement should keep entries visible in map output.
# RUN: %echo "SECTIONS {" > %t.place.t
# RUN: %echo "  .text : { *(.text .text.*) }" >> %t.place.t
# RUN: %echo "  .tbl : { *(.riscv.jvt) }" >> %t.place.t
# RUN: %echo "}" >> %t.place.t
# RUN: %link %linkopts %t.o --relax-tbljal -e _start \
# RUN:   --defsym live=0x150000 --defsym dead=0x160000 \
# RUN:   -T %t.place.t -MapStyle txt -Map %t.place.map -o %t.place
# RUN: %filecheck --check-prefix=PLACE %s < %t.place.map
#
## Linker script discard of .riscv.jvt must prevent table-jump relaxation.
# RUN: %echo "SECTIONS {" > %t.discard.t
# RUN: %echo "  /DISCARD/ : { *(.riscv.jvt) }" >> %t.discard.t
# RUN: %echo "}" >> %t.discard.t
# RUN: %link %linkopts %t.o --relax-tbljal -e _start \
# RUN:   --defsym live=0x150000 --defsym dead=0x160000 \
# RUN:   -T %t.discard.t -MapStyle txt -Map %t.discard.map -o %t.discard
# RUN: %filecheck --check-prefix=DISCARD %s < %t.discard.map
# RUN: %objdump -d -M no-aliases --mattr=+zcmt --no-show-raw-insn %t.discard \
# RUN:   | %filecheck --check-prefix=DISCARD-DISASM %s
#
# PARTIAL-NOT: .riscv.jvt
# PARTIAL-NOT: .riscv.jvt entries
#
# GC: .riscv.jvt
# GC: #{{[ \t]+}}.riscv.jvt entries:
# GC: cm.jt{{[ \t]+}}live{{[ \t]+}}0x150000
# GC-NOT: cm.jt{{[ \t]+}}dead
#
# PLACE: .tbl
# PLACE: #{{[ \t]+}}.riscv.jvt entries:
# PLACE: cm.jt{{[ \t]+}}live{{[ \t]+}}0x150000
# PLACE: cm.jt{{[ \t]+}}dead{{[ \t]+}}0x160000
#
# DISCARD: /DISCARD/
# DISCARD: *(.riscv.jvt)
# DISCARD-NOT: .riscv.jvt entries
# DISCARD-DISASM-NOT: cm.jt
# DISCARD-DISASM-NOT: cm.jalt

.section .text.start, "ax", @progbits
.global _start
.p2align 2
_start:
  call live_func
  tail live
  tail live

.section .text.live, "ax", @progbits
.global live_func
.p2align 2
live_func:
  tail live
  tail live
  ret

.section .text.dead, "ax", @progbits
.global dead_func
.p2align 2
dead_func:
  tail dead
  tail dead
  ret
