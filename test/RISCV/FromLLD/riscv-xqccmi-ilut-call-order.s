## Verify ILUT is applied before call relaxation so recorded ILUT offsets remain valid.
#
# REQUIRES: riscv32
#
# RUN: %llvm-mc -filetype=obj -mattr=+relax,+experimental-xqccmi,+xqcilia %s -o %t.o
# RUN: %link %linkopts %t.o --relax-ilut -e _start -o %t
# RUN: %objdump -d --section=.text -M no-aliases --mattr=+experimental-xqccmi --no-show-raw-insn %t \
# RUN:   | %filecheck %s
# RUN: %readelf -S %t | %filecheck --check-prefix=SECTION %s
# RUN: %link %linkopts %t.o --no-relax-ilut --no-relax -e _start -o %t.no
# RUN: %objdump -d --section=.text -M no-aliases --mattr=+experimental-xqccmi --no-show-raw-insn %t.no \
# RUN:   | %filecheck --check-prefix=NO-RELAX %s
#
# CHECK-COUNT-2: c.j
# CHECK-COUNT-6: qc.cm.ilut
# CHECK-NOT: addi a0, a0, 0x1
# CHECK-NOT: addi a1, a1, 0x2
# SECTION: .text {{.*}} 000012 {{.*}}
# NO-RELAX-NOT: qc.cm.ilut
# NO-RELAX: addi a0, a0, 0x1
# NO-RELAX: addi a1, a1, 0x2

.option nopic
.attribute 4, 16

.text
.global _start
.global target
_start:
  call target
  call target

  .option push
  .option norvc
  .rept 6
  addi a0, a0, 1
  addi a1, a1, 2
  .endr
  .option pop

target:
  ret
