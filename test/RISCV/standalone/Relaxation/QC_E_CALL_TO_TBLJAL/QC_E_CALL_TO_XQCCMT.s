#----------QC_E_CALL_TO_XQCCMT.s----------------- Executable------------------#
# BEGIN_COMMENT
# Relax near QC.E.JAL/QC.E.J (Xqcilb 48-bit) to C.JAL/C.J when possible.
# Relax far profitable calls to QC.CM.JALT/QC.CM.JT (Xqccmt 16-bit) only
# when compressed relaxation is enabled.
# END_COMMENT
#--------------------------------------------------------------------
# REQUIRES: riscv32

# RUN: %llvm-mc -filetype=obj -mattr=+relax,+c,+xqcilb,+experimental-xqccmt \
# RUN:   --defsym NEAR_TARGET=1 %s -o %t.near.o
# RUN: %link %linkopts --relax-xqci --relax-tbljal=xqccmt %t.near.o \
# RUN:   -o %t.near.out
# RUN: %objdump -d -M no-aliases --mattr=+experimental-xqccmt \
# RUN:   %t.near.out 2>&1 \
# RUN:   | %filecheck %s --check-prefix=NEAR-DISASM
# NEAR-DISASM: c.jal
# NEAR-DISASM: c.j
# NEAR-DISASM-NOT: qc.cm.jalt
# NEAR-DISASM-NOT: qc.cm.jt
# NEAR-DISASM-NOT: qc.e.jal
# NEAR-DISASM-NOT: qc.e.j
# RUN: %readelf -S %t.near.out \
# RUN:   | %filecheck %s --check-prefix=NO-JVT

# RUN: %llvm-mc -filetype=obj -mattr=+relax,+c,+xqcilb,+experimental-xqccmt \
# RUN:   --defsym CALL_COUNT=34 --defsym TAIL_COUNT=34 %s -o %t.far.o
# RUN: %link %linkopts --relax-xqci --relax-tbljal=xqccmt \
# RUN:   --defsym target=0x800000 %t.far.o -o %t.far.out \
# RUN:   --verbose 2>&1 \
# RUN:   | %filecheck %s --check-prefix=VERBOSE-XQCCMT
# VERBOSE-XQCCMT-COUNT-68: RISCV_TBJAL : Deleting 4 bytes
# RUN: %objdump -d -M no-aliases --mattr=+experimental-xqccmt \
# RUN:   %t.far.out 2>&1 \
# RUN:   | %filecheck %s --check-prefix=XQCCMT-DISASM
# XQCCMT-DISASM-COUNT-34: qc.cm.jalt
# XQCCMT-DISASM-COUNT-34: qc.cm.jt
# XQCCMT-DISASM-NOT: qc.e.jal
# XQCCMT-DISASM-NOT: qc.e.j
# RUN: %readelf -S %t.far.out \
# RUN:   | %filecheck %s --check-prefix=JVT
# JVT: .riscv.jvt PROGBITS

# RUN: %link %linkopts --relax-xqci --relax-tbljal=xqccmt --no-relax-c \
# RUN:   --defsym target=0x90000 %t.far.o -o %t.no-relax-c.out
# RUN: %objdump -d -M no-aliases --mattr=+experimental-xqccmt \
# RUN:   %t.no-relax-c.out 2>&1 \
# RUN:   | %filecheck %s --check-prefix=NO-RELAX-C-DISASM \
# RUN:   --implicit-check-not=c.jal --implicit-check-not=c.j \
# RUN:   --implicit-check-not=qc.cm.jalt --implicit-check-not=qc.cm.jt \
# RUN:   --implicit-check-not=qc.e.jal --implicit-check-not=qc.e.j
# NO-RELAX-C-DISASM-COUNT-68: jal
# RUN: %readelf -S %t.no-relax-c.out \
# RUN:   | %filecheck %s --check-prefix=NO-JVT

# RUN: %link %linkopts --relax-xqci --relax-tbljal=xqccmt \
# RUN:   --defsym target=0x100001000 %t.far.o -o %t.out-of-u32.out
# RUN: %objdump -d -M no-aliases --mattr=+experimental-xqccmt \
# RUN:   %t.out-of-u32.out 2>&1 \
# RUN:   | %filecheck %s --check-prefix=OUT-OF-U32-DISASM \
# RUN:   --implicit-check-not=qc.cm.jalt --implicit-check-not=qc.cm.jt
# OUT-OF-U32-DISASM-COUNT-34: qc.e.jal
# OUT-OF-U32-DISASM-COUNT-34: qc.e.j
# RUN: %readelf -S %t.out-of-u32.out \
# RUN:   | %filecheck %s --check-prefix=NO-JVT

# RUN: %echo "SECTIONS {" > %t.discard.t
# RUN: %echo "  /DISCARD/ : { *(.riscv.jvt) }" >> %t.discard.t
# RUN: %echo "}" >> %t.discard.t
# RUN: %link %linkopts --relax-xqci --relax-tbljal=xqccmt \
# RUN:   --defsym target=0x90000 %t.far.o -T %t.discard.t \
# RUN:   -o %t.discard.out
# RUN: %objdump -d -M no-aliases --mattr=+experimental-xqccmt \
# RUN:   %t.discard.out 2>&1 \
# RUN:   | %filecheck %s --check-prefix=NO-JVT-DISASM
# RUN: %readelf -S %t.discard.out \
# RUN:   | %filecheck %s --check-prefix=NO-JVT

# NO-JVT-DISASM-NOT: qc.cm.jalt
# NO-JVT-DISASM-NOT: qc.cm.jt
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
