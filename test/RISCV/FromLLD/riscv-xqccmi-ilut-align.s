## ILUT must leave bytes reserved by R_RISCV_ALIGN for the alignment pass.
#
# REQUIRES: riscv32
#
# RUN: %llvm-mc -filetype=obj -triple=riscv32-unknown-elf \
# RUN:   -mattr=+relax,+experimental-xqccmi %s -o %t.o
# RUN: %readelf -r %t.o | %filecheck --check-prefix=RELOCS %s
# RUN: %link %linkopts %t.o --relax-ilut -e _start -o %t
# RUN: %objdump -d --section=.text -M no-aliases \
# RUN:   --mattr=+experimental-xqccmi --no-show-raw-insn %t \
# RUN:   | %filecheck %s
#
# RELOCS: R_RISCV_ALIGN
#
# CHECK-LABEL: <_start>:
# CHECK-NEXT: addi a0, a0, 0x1
# CHECK-NOT: qc.cm.ilut
# CHECK: {{[0-9a-f]*[048c]0}} <target>:

.text
.globl _start, target
_start:
  .option relax
  .option norvc
  addi a0, a0, 1
  call target
  .align 6
target:
  ret
