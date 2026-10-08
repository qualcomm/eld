## Do not create a table when its cost exactly cancels the instruction savings.
## Four near tails save 2 bytes each on RV64, which exactly pays for one JVT
## entry. The transformation is still profitable on RV32.
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+zcmt %s -o %t.zcmt.o
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+experimental-xqccmt %s -o %t.xqccmt.o
# RUN: %link %linkopts --relax-tbljal --no-relax-c --section-start .text=0x2000 -e _start %t.zcmt.o -o %t.zcmt
# RUN: %objdump -d -M no-aliases --mattr=+zcmt --no-show-raw-insn %t.zcmt \
# RUN:   | %filecheck --check-prefix=ZCMT%xlen %s
# RUN: %link %linkopts --relax-tbljal=xqccmt --no-relax-c --section-start .text=0x2000 -e _start %t.xqccmt.o -o %t.xqccmt
# RUN: %objdump -d -M no-aliases --mattr=+experimental-xqccmt --no-show-raw-insn %t.xqccmt \
# RUN:   | %filecheck --check-prefix=XQCCMT%xlen %s
#
# ZCMT4: cm.jt
# ZCMT8-NOT: cm.jt
# ZCMT8-NOT: cm.jalt
# XQCCMT4: qc.cm.jt
# XQCCMT8-NOT: qc.cm.jt
# XQCCMT8-NOT: qc.cm.jalt

.text
.globl _start
.p2align 2
_start:
  tail target
  tail target
  tail target
  tail target

.globl target
.p2align 2
target:
  ret
