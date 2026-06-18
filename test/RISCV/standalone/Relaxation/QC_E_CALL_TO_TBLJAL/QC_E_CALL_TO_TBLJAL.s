#----------QC_E_CALL_TO_TBLJAL.s----------------- Executable------------------#
# BEGIN_COMMENT
# Relax near QC.E.JAL/QC.E.J (Xqcilb 48-bit) to C.JAL/C.J when possible.
# Relax far profitable calls to CM.JALT/CM.JT (Zcmt 16-bit) only when
# compressed relaxation is enabled.
# END_COMMENT
#--------------------------------------------------------------------
# REQUIRES: riscv32

# RUN: %llvm-mc -filetype=obj -mattr=+relax,+c,+xqcilb \
# RUN:   --defsym NEAR_TARGET=1 %s -o %t.near.o
# RUN: %link %linkopts --relax-xqci --relax-tbljal %t.near.o \
# RUN:   -o %t.near.out
# RUN: %objdump -d -M no-aliases --mattr=+zcmt %t.near.out 2>&1 \
# RUN:   | %filecheck %s --check-prefix=NEAR-DISASM
# NEAR-DISASM: c.jal
# NEAR-DISASM: c.j
# NEAR-DISASM-NOT: cm.jalt
# NEAR-DISASM-NOT: cm.jt
# NEAR-DISASM-NOT: qc.e.jal
# NEAR-DISASM-NOT: qc.e.j
# RUN: %readelf -S %t.near.out \
# RUN:   | %filecheck %s --check-prefix=NO-JVT

# RUN: %llvm-mc -filetype=obj -mattr=+relax,+c,+xqcilb \
# RUN:   --defsym CALL_COUNT=34 --defsym TAIL_COUNT=34 %s -o %t.far.o
# RUN: %link %linkopts --relax-xqci --relax-tbljal \
# RUN:   --defsym target=0x150000 %t.far.o -o %t.far.out \
# RUN:   --verbose 2>&1 \
# RUN:   | %filecheck %s --check-prefix=VERBOSE-TBLJAL
# VERBOSE-TBLJAL-COUNT-68: RISCV_TBJAL : Deleting 4 bytes
# RUN: %objdump -d -M no-aliases --mattr=+zcmt %t.far.out 2>&1 \
# RUN:   | %filecheck %s --check-prefix=TBLJAL-DISASM
# TBLJAL-DISASM-COUNT-34: cm.jalt
# TBLJAL-DISASM-COUNT-34: cm.jt
# TBLJAL-DISASM-NOT: qc.e.jal
# TBLJAL-DISASM-NOT: qc.e.j
# RUN: %readelf -S %t.far.out \
# RUN:   | %filecheck %s --check-prefix=JVT
# JVT: .riscv.jvt PROGBITS

# RUN: %link %linkopts --relax-xqci --relax-tbljal --no-relax-c \
# RUN:   --defsym target=0x90000 %t.far.o -o %t.no-relax-c.out
# RUN: %objdump -d -M no-aliases --mattr=+zcmt \
# RUN:   %t.no-relax-c.out 2>&1 \
# RUN:   | %filecheck %s --check-prefix=NO-RELAX-C-DISASM \
# RUN:   --implicit-check-not=c.jal --implicit-check-not=c.j \
# RUN:   --implicit-check-not=cm.jalt --implicit-check-not=cm.jt \
# RUN:   --implicit-check-not=qc.e.jal --implicit-check-not=qc.e.j
# NO-RELAX-C-DISASM-COUNT-68: jal

# RUN: %echo "SECTIONS {" > %t.discard.t
# RUN: %echo "  /DISCARD/ : { *(.riscv.jvt) }" >> %t.discard.t
# RUN: %echo "}" >> %t.discard.t
# RUN: %link %linkopts --relax-xqci --relax-tbljal \
# RUN:   --defsym target=0x90000 %t.far.o -T %t.discard.t \
# RUN:   -o %t.discard.out
# RUN: %objdump -d -M no-aliases --mattr=+zcmt %t.discard.out 2>&1 \
# RUN:   | %filecheck %s --check-prefix=NO-JVT-DISASM
# RUN: %readelf -S %t.discard.out \
# RUN:   | %filecheck %s --check-prefix=NO-JVT

# NO-JVT-DISASM-NOT: cm.jalt
# NO-JVT-DISASM-NOT: cm.jt
# NO-JVT-DISASM-COUNT-68: jal


# NO-JVT-NOT: .riscv.jvt

  .option exact
  .option relax

  .text
  .align 1
  .globl _start
  .type _start, @function
_start:
.ifdef NEAR_TARGET
  qc.e.jal target
  qc.e.j target

  .align 1
  .type target, @function
target:
  ret
  .size target, .-target
.else
  .rept CALL_COUNT
  qc.e.jal target
  .endr
  .rept TAIL_COUNT
  qc.e.j target
  .endr
.endif
  .size _start, .-_start
